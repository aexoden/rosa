project('rosa', 'cpp',
    version : '1.2.0',
    meson_version : '>=0.45.0',
    default_options : [
        'buildtype=release',
        'warning_level=3',
        'b_lto=true',
        'cpp_std=c++17'
    ]
)

project_dependencies = [
    dependency('boost'),
    dependency('lmdb')
]

subdir('external')
subdir('src')

executable(
    meson.project_name(),
    main_sources,
    main_vcs,
    dependencies : project_dependencies,
    include_directories : external_inc
)
