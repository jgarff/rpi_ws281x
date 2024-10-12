project('all', 'c', default_options: ['werror=true', 'buildtype=debugoptimized'])

install_headers('rp1_ws281x_pwm.h')

executable('ws281x_pwm_cmd',
           files('main.c'),
           dependencies : [],
           install: true,
           link_with : [])

