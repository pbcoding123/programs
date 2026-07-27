import time
import requests


def get_text():
    return requests.get("https://v1.hitokoto.cn/?encode=text", timeout=2).text


while True:
    time.sleep(1.2)
    print(get_text())
