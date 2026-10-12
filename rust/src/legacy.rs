//! Importer for the original tab-separated data files.
//!
//! This is one importer among potentially several: it turns the old route,
//! map and encounter files into the format-neutral types in [`crate::model`]
//! and [`crate::data`]. Unlike the C++ reader, it rejects malformed lines with
//! a line number instead of warning and carrying on.
//!
//! Fields are separated by one or more tabs, so empty fields cannot be
//! expressed (this matches the C++ reader's `token_compress_on`). Lines that
//! are empty or start with `#` are ignored.

use std::collections::HashMap;
use std::fmt;
use std::fs;
use std::path::Path as FsPath;

use crate::data::{BattleDuration, Formation, GameData, MapInfo};
use crate::model::{
	ChoiceOption, Choice, FormationId, MAX_SEARCH_TARGETS, MapId, Member, Node, PARTY_SLOTS, Party, Path, Route,
	Search, SearchExpr, VariableId,
};
use crate::rng::GROUP_SLOTS;
use crate::time::Milliframes;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ParseError {
	pub source: String,
	pub line: usize,
	pub message: String,
}

impl fmt::Display for ParseError {
	fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
		write!(f, "{}:{}: {}", self.source, self.line, self.message)
	}
}

impl std::error::Error for ParseError {}

type FieldResult<T> = Result<T, String>;

fn fields(line: &str) -> Vec<&str> {
	line.split('\t').filter(|field| !field.is_empty()).collect()
}

fn content_lines(text: &str) -> impl Iterator<Item = (usize, &str)> {
	text.lines()
		.enumerate()
		.map(|(index, line)| (index + 1, line))
		.filter(|(_, line)| !line.is_empty() && !line.starts_with('#'))
}

fn number<T: std::str::FromStr>(field: &str, what: &str) -> FieldResult<T> {
	field.parse().map_err(|_| format!("invalid {what}: {field:?}"))
}

fn hex<T: TryFrom<u32>>(field: &str, what: &str) -> FieldResult<T> {
	u32::from_str_radix(field, 16)
		.ok()
		.and_then(|value| T::try_from(value).ok())
		.ok_or_else(|| format!("invalid {what}: {field:?}"))
}

fn flag(field: &str) -> FieldResult<bool> {
	match field {
		"+" => Ok(true),
		"-" => Ok(false),
		_ => Err(format!("expected + or -, found {field:?}")),
	}
}

/// Parses a variable name. `-` means none; so does 0, which the C++ solver
/// never treats as a variable.
fn variable(field: &str) -> FieldResult<Option<VariableId>> {
	if field == "-" {
		return Ok(None);
	}

	let id: u32 = hex(field, "variable")?;
	Ok((id != 0).then_some(VariableId(id)))
}

fn fractional_frames(field: &str) -> FieldResult<Milliframes> {
	Ok(Milliframes::from_fractional_frames(number(field, "frame count")?))
}

/// Parses party notation such as `3G+K11---C11------10`: front row size,
/// `G` if the party has GP, `+` if on the world map, five three-character
/// slots (a character letter and two-digit agility, or `---`), and a
/// two-digit level.
pub fn parse_party(text: &str) -> FieldResult<Party> {
	let bytes = text.as_bytes();

	if bytes.len() != 3 + 3 * PARTY_SLOTS + 2 || !text.is_ascii() {
		return Err(format!("party must be 20 ASCII characters: {text:?}"));
	}

	let mut members = [Member::default(); PARTY_SLOTS];

	for (slot, member) in members.iter_mut().enumerate() {
		let start = 3 + slot * 3;
		let agility = &text[start + 1..start + 3];

		member.character = match bytes[start] {
			b'-' => 0,
			c @ b'A'..=b'Z' => c - b'A' + 1,
			c @ b'a'..=b'z' => c - b'a' + 27,
			_ => return Err(format!("invalid character in party {text:?}")),
		};

		member.agility = if agility == "--" { 0 } else { number(agility, "agility")? };
	}

	Ok(Party {
		three_front: bytes[0] == b'3',
		has_gp: bytes[1] == b'G',
		on_world_map: bytes[2] == b'+',
		level: number(&text[18..20], "level")?,
		members,
	})
}

/// Formats a party in the notation [`parse_party`] reads.
#[must_use]
pub fn party_notation(party: &Party) -> String {
	let mut text = String::with_capacity(20);
	text.push(if party.three_front { '3' } else { '2' });
	text.push(if party.has_gp { 'G' } else { '-' });
	text.push(if party.on_world_map { '+' } else { '-' });

	for member in &party.members {
		match member.character {
			0 if member.agility == 0 => text.push_str("---"),
			0 => text.push_str(&format!("-{:02}", member.agility)),
			c => {
				let letter = if c <= 26 { b'A' + c - 1 } else { b'a' + c - 27 };
				text.push(char::from(letter));
				text.push_str(&format!("{:02}", member.agility));
			}
		}
	}

	text.push_str(&format!("{:02}", party.level));
	text
}

/// Parses a search expression such as `13|15|16` or `(1+2)>3`, where the
/// numbers are formation IDs. Returns the targets in order of appearance and
/// an expression that refers to them by index.
pub fn parse_search_expression(text: &str) -> FieldResult<(Vec<FormationId>, SearchExpr)> {
	struct Parser<'a> {
		text: &'a [u8],
		position: usize,
		targets: Vec<FormationId>,
	}

	impl Parser<'_> {
		fn peek(&mut self) -> Option<u8> {
			while self.text.get(self.position) == Some(&b' ') {
				self.position += 1;
			}

			self.text.get(self.position).copied()
		}

		fn list(&mut self, separator: u8, make: fn(Vec<SearchExpr>) -> SearchExpr, item: fn(&mut Self) -> FieldResult<SearchExpr>) -> FieldResult<SearchExpr> {
			let mut nodes = vec![item(self)?];

			while self.peek() == Some(separator) {
				self.position += 1;
				nodes.push(item(self)?);
			}

			Ok(SearchExpr::group(make, nodes))
		}

		fn sequence(&mut self) -> FieldResult<SearchExpr> {
			self.list(b'>', SearchExpr::Sequence, Self::disjunction)
		}

		fn disjunction(&mut self) -> FieldResult<SearchExpr> {
			self.list(b'|', SearchExpr::Any, Self::conjunction)
		}

		fn conjunction(&mut self) -> FieldResult<SearchExpr> {
			self.list(b'+', SearchExpr::All, Self::unit)
		}

		fn unit(&mut self) -> FieldResult<SearchExpr> {
			match self.peek() {
				Some(b'(') => {
					self.position += 1;
					let inner = self.sequence()?;

					if self.peek() != Some(b')') {
						return Err("expected ')'".into());
					}

					self.position += 1;
					Ok(inner)
				}
				Some(b'0'..=b'9') => {
					let start = self.position;

					while self.text.get(self.position).is_some_and(u8::is_ascii_digit) {
						self.position += 1;
					}

					let digits = std::str::from_utf8(&self.text[start..self.position]).unwrap_or_default();
					self.targets.push(FormationId(number(digits, "formation")?));

					if self.targets.len() > MAX_SEARCH_TARGETS {
						return Err(format!("more than {MAX_SEARCH_TARGETS} search targets"));
					}

					Ok(SearchExpr::Target(self.targets.len() - 1))
				}
				_ => Err("expected a formation number or '('".into()),
			}
		}
	}

	let mut parser = Parser { text: text.as_bytes(), position: 0, targets: Vec::new() };
	let expression = parser.sequence().map_err(|e| format!("{e} in search expression {text:?}"))?;

	if parser.peek().is_some() {
		return Err(format!("unexpected text in search expression {text:?}"));
	}

	Ok((parser.targets, expression))
}

struct OpenChoice {
	variable: Option<VariableId>,
	expected_options: usize,
	options: Vec<ChoiceOption>,
}

/// Parses a route file.
pub fn parse_route(text: &str, source: &str) -> Result<Route, ParseError> {
	let mut route = Route { data_set: "ff2us".into(), ..Route::default() };
	let mut body: Vec<Node> = Vec::new();
	let mut choices: Vec<(OpenChoice, Vec<Node>)> = Vec::new();

	for (line_number, line) in content_lines(text) {
		parse_route_line(line, &mut route, &mut body, &mut choices)
			.map_err(|message| ParseError { source: source.into(), line: line_number, message })?;
	}

	if !choices.is_empty() {
		return Err(ParseError { source: source.into(), line: text.lines().count(), message: "CHOICE without END".into() });
	}

	route.body = body;
	Ok(route)
}

fn parse_route_line(line: &str, route: &mut Route, body: &mut Vec<Node>, choices: &mut Vec<(OpenChoice, Vec<Node>)>) -> FieldResult<()> {
	let tokens = fields(line);

	let node = match tokens.as_slice() {
		["ROUTE", title] => {
			(*title).clone_into(&mut route.title);
			return Ok(());
		}
		["VERSION", version] => {
			route.version = number(version, "version")?;
			return Ok(());
		}
		["DATA", data_set] => {
			(*data_set).clone_into(&mut route.data_set);
			return Ok(());
		}
		["NOTE", text] => Node::Note { text: (*text).into() },
		["PARTY", party] => Node::Party { party: parse_party(party)? },
		["DELAY", frames] => Node::Delay { frames: number(frames, "delay")? },
		["PATH", var, map, tiles, required, optional, transitions, single, double, save, penalty] => Node::Path(Path {
			variable: variable(var)?,
			map: MapId(hex(map, "map")?),
			tiles: number(tiles, "tile count")?,
			required_steps: number(required, "required steps")?,
			optional_steps: number(optional, "optional steps")?,
			transitions: number(transitions, "transition count")?,
			can_single_step: flag(single)?,
			can_double_step: flag(double)?,
			can_step_during_save: flag(save)?,
			first_battle_penalty: fractional_frames(penalty)?,
			ends_search: false,
		}),
		["SEARCH", label, expression, party] => {
			let (targets, expression) = parse_search_expression(expression)?;
			Node::Search(Search { label: (*label).into(), targets, expression, party: parse_party(party)? })
		}
		["WAIT"] => {
			return match body.last_mut() {
				Some(Node::Path(path)) => {
					path.ends_search = true;
					Ok(())
				}
				_ => Err("WAIT must follow a PATH".into()),
			};
		}
		["CHOICE", var, count] => {
			let choice = OpenChoice { variable: variable(var)?, expected_options: number(count, "option count")?, options: Vec::new() };
			choices.push((choice, std::mem::take(body)));
			return Ok(());
		}
		["OPTION", label] => {
			let Some((choice, _)) = choices.last_mut() else {
				return Err("OPTION outside of a CHOICE".into());
			};

			close_option(choice, body)?;
			choice.options.push(ChoiceOption { label: (*label).into(), body: Vec::new() });
			return Ok(());
		}
		["END"] => {
			let Some((mut choice, outer)) = choices.pop() else {
				return Err("END outside of a CHOICE".into());
			};

			close_option(&mut choice, body)?;

			if choice.options.len() != choice.expected_options {
				return Err(format!("CHOICE declares {} options but has {}", choice.expected_options, choice.options.len()));
			}

			*body = outer;
			Node::Choice(Choice { variable: choice.variable, options: choice.options })
		}
		[keyword, ..] => return Err(format!("unrecognized or malformed {keyword} instruction")),
		[] => return Ok(()),
	};

	body.push(node);
	Ok(())
}

fn close_option(choice: &mut OpenChoice, body: &mut Vec<Node>) -> FieldResult<()> {
	match choice.options.last_mut() {
		Some(option) => option.body = std::mem::take(body),
		None if body.is_empty() => {}
		None => return Err("instructions between CHOICE and its first OPTION".into()),
	}

	Ok(())
}

/// Parses a map data file.
pub fn parse_maps(text: &str, source: &str, data: &mut GameData) -> Result<(), ParseError> {
	for (line_number, line) in content_lines(text) {
		let error = |message| ParseError { source: source.into(), line: line_number, message };

		match fields(line).as_slice() {
			["MAP", id, rate, group, title, description] => {
				let id = MapId(hex(id, "map").map_err(error)?);
				let map = MapInfo {
					encounter_rate: number(rate, "encounter rate").map_err(error)?,
					encounter_group: number(group, "encounter group").map_err(error)?,
					title: (*title).into(),
					description: (*description).into(),
				};

				if data.maps.insert(id, map).is_some() {
					return Err(error(format!("duplicate map {:04X}", id.0)));
				}
			}
			_ => return Err(error("unrecognized line".into())),
		}
	}

	Ok(())
}

/// Parses an encounter data file.
pub fn parse_encounters(text: &str, source: &str, data: &mut GameData) -> Result<(), ParseError> {
	for (line_number, line) in content_lines(text) {
		let error = |message| ParseError { source: source.into(), line: line_number, message };

		match fields(line).as_slice() {
			["ENCOUNT", id, description, party, average, minimum] => {
				let id = FormationId(number(id, "formation").map_err(error)?);
				let party = parse_party(party).map_err(error)?;
				let duration = BattleDuration {
					average: fractional_frames(average).map_err(error)?,
					minimum: fractional_frames(minimum).map_err(error)?,
				};

				let formation = data
					.formations
					.entry(id)
					.or_insert_with(|| Formation { description: (*description).into(), durations: Vec::new() });

				match formation.durations.iter_mut().find(|(p, _)| *p == party) {
					Some((_, existing)) => *existing = duration,
					None => formation.durations.push((party, duration)),
				}
			}
			["GROUP", id, slots @ ..] if slots.len() == GROUP_SLOTS => {
				let id = number(id, "group").map_err(error)?;
				let mut group = [FormationId(0); GROUP_SLOTS];

				for (target, slot) in group.iter_mut().zip(slots) {
					*target = FormationId(number(slot, "formation").map_err(error)?);
				}

				data.groups.insert(id, group);
			}
			_ => return Err(error("unrecognized line".into())),
		}
	}

	Ok(())
}

/// Everything needed to run one route, loaded from the original directory
/// layout (`routes/NAME.txt`, `maps/SET.txt`, `encounters/SET.txt`).
pub struct Loaded {
	pub route: Route,
	pub data: GameData,
}

/// Loads a route by name from a data directory laid out like `data/`.
///
/// # Errors
///
/// Returns an error if a file is missing or malformed.
pub fn load(data_dir: &FsPath, route_name: &str) -> Result<Loaded, Box<dyn std::error::Error>> {
	let read = |path: std::path::PathBuf| fs::read_to_string(&path).map(|text| (text, path.display().to_string())).map_err(|e| format!("{}: {e}", path.display()));

	let (text, source) = read(data_dir.join("routes").join(format!("{route_name}.txt")))?;
	let route = parse_route(&text, &source)?;

	let mut data = GameData { maps: HashMap::new(), ..GameData::default() };
	let (text, source) = read(data_dir.join("maps").join(format!("{}.txt", route.data_set)))?;
	parse_maps(&text, &source, &mut data)?;
	let (text, source) = read(data_dir.join("encounters").join(format!("{}.txt", route.data_set)))?;
	parse_encounters(&text, &source, &mut data)?;

	Ok(Loaded { route, data })
}

#[cfg(test)]
mod tests {
	use super::*;

	#[test]
	fn party_round_trips() {
		for text in ["3G+K11---C11------10", "2G-R24G30f20c11d1434", "3G+---------------01"] {
			assert_eq!(party_notation(&parse_party(text).unwrap()), text);
		}
	}

	#[test]
	fn empty_party_equals_default() {
		assert_eq!(parse_party("3G+---------------01").unwrap(), Party::default());
	}

	#[test]
	fn search_expression_collapses_single_groups() {
		let (targets, expression) = parse_search_expression("200").unwrap();
		assert_eq!(targets, vec![FormationId(200)]);
		assert_eq!(expression, SearchExpr::Target(0));

		let (targets, expression) = parse_search_expression("13|15|16").unwrap();
		assert_eq!(targets.len(), 3);
		assert_eq!(expression, SearchExpr::Any(vec![SearchExpr::Target(0), SearchExpr::Target(1), SearchExpr::Target(2)]));

		let (_, expression) = parse_search_expression("(1 + (2)) > 3|4").unwrap();
		assert_eq!(
			expression,
			SearchExpr::Sequence(vec![
				SearchExpr::All(vec![SearchExpr::Target(0), SearchExpr::Target(1)]),
				SearchExpr::Any(vec![SearchExpr::Target(2), SearchExpr::Target(3)]),
			])
		);
	}

	#[test]
	fn choices_nest() {
		let text = "PATH\t-\t0002\t1\t1\t0\t0\t-\t+\t-\t0\nCHOICE\tC1\t2\nOPTION\tA\nNOTE\tx\nOPTION\tB\nCHOICE\tC2\t1\nOPTION\tC\nEND\nEND\nNOTE\ty\n";
		let route = parse_route(text, "test").unwrap();
		assert_eq!(route.body.len(), 3);

		let Node::Choice(choice) = &route.body[1] else { panic!() };
		assert_eq!(choice.variable, Some(VariableId(0xC1)));
		assert_eq!(choice.options[0].body.len(), 1);
		assert!(matches!(choice.options[1].body[0], Node::Choice(_)));
	}

	#[test]
	fn errors_carry_line_numbers() {
		let error = parse_route("# comment\nPATH\tbad\n", "test").unwrap_err();
		assert_eq!(error.line, 2);
	}
}
