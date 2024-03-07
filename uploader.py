import requests
import sys
import traceback

try:
    sys.stdin.reconfigure(encoding='utf-8')
    filename = sys.argv[1]

    with open(filename, 'rb') as f:
        data = {
            'reqtype': (None, 'fileupload'),
            'time': (None, '1h'),
            # 'userhash': (None, ''),
            'fileToUpload': f
        }
        r = requests.post("https://litterbox.catbox.moe/resources/internals/api.php", files=data)

    if not r.ok:
        print(r.text[:1000])
        exit(1)

    print(r.text, end='')

except:
    traceback.print_exc(file=sys.stdout)
    exit(1)