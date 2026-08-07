project('craft', 'c',
        version : '1.0.0',
        default_options : ['warning_level=3', 'c_std=c99'])

craft = executable('craft',
                   'src/main.c',
                   'src/commands/help.c',
                   'src/app.c',
                   install : true)

test('test', craft)