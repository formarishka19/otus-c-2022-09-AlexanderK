import time
from datetime import datetime

logfile = 'test.log'
i = 0
while True:
    now = datetime.now()
    date_time_str = now.strftime("%Y-%m-%d %H:%M:%S")
    with open(logfile, 'a') as f:
        f.write(f'{date_time_str} -- test record\n')
    time.sleep(1)
    i += 1
    if (i > 30):
        with open(logfile, 'w') as f:
            f.flush()
        i = 0