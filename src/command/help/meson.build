help_auto_h = custom_target(
    'help.auto.h',
    output : 'help.auto.h',
    depend_files: [
        '../../build/config/config.yaml',
        '../../build/help/help.xml',
    ],
    command : [build_help, '@SOURCE_ROOT@'],
)
