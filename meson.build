project('craft', 'c',
        version : '1.0.0',
        default_options : ['warning_level=3', 'c_std=c99'])

include = include_directories('src')

craft = executable('craft',
                   'src/main.c',
                   'src/app.c',
                   'src/commands/help.c',
                   'src/commands/version.c',
                   'src/commands/build.c',
                   'src/build/manifest.c',
                   'src/toml/lexer.c',
                   'src/toml/parser.c',
                   'src/toml/toml.c',
                   include_directories : include,
                   install : true
)

test('test', craft)