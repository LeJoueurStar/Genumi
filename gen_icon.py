import sys
data = open(sys.argv[1], 'rb').read()
body = ','.join(str(b) for b in data)
print('const unsigned char eadk_app_icon[] __attribute__((section(".rodata.eadk_app_icon"))) = {' + body + '};')
