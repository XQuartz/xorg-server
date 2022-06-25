xcb_dep = dependency('xcb', required: false)
xcb_image_dep = dependency('xcb-image', required: false)
xcb_util_dep = dependency('xcb-util', required: false)

if get_option('xvfb')
    xvfb_args = [
        xvfb_server.full_path(),
        '-screen',
        'scrn',
        '1280x1024x24'
    ]

    if xcb_dep.found() and xcb_image_dep.found() and xcb_util_dep.found() and get_option('xvfb') and get_option('xephyr') and build_glamor
        bug1354 = executable('bug1354', 'bug1354.c', dependencies: [xcb_dep, xcb_image_dep, xcb_util_dep])
        test('bug1354-gl',
                simple_xinit,
                args: [simple_xinit.full_path(),
                    bug1354.full_path(),
                    '-t',':201','-r',':200',
                    '----',
                    xephyr_server.full_path(),
                    '-glamor',
                    '-schedMax', '2000',
                    ':201',
                    '--',
                    xvfb_args,
                    ':200'
                    ],
                suite: 'xephyr-glamor',
                timeout: 300,
            )
        test('bug1354-gles',
                simple_xinit,
                args: [simple_xinit.full_path(),
                    bug1354.full_path(),
                    '-t',':199','-r',':198',
                    '----',
                    xephyr_server.full_path(),
                    '-glamor_gles2',
                    '-schedMax', '2000',
                    ':199',
                    '--',
                    xvfb_args,
                    ':198'
                    ],
                suite: 'xephyr-glamor-gles2',
                timeout: 300,
            )
    endif
endif