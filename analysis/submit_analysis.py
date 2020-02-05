#!/usr/bin/python

import sys
import getpass
import requests
import datetime

# guess API url from current time 
def guess_api_url():
    now = datetime.datetime.now()
    semester = "spring" if now.month < 7 else "fall"
    return "https://courses.cs.vt.edu/cs3214/autograder_api/%s%d" % (semester, now.year)

API_URL = guess_api_url()

def submit_file(tag, file, api_url, cookies):
    r = requests.post(api_url + "/upload/" + str(tag), cookies=cookies, files={'uploadedfile': open(file, 'rb')})
    return r


def authenticate(api_url, user, password):
    r = requests.post(api_url + "/login", json={'username': user, 'password': password})
    if r.status_code == requests.codes.ok:
        return r.cookies
    # could not authenticate
    return None


def usage():
    print("Usage {} <filename>".format(sys.argv[0]))
    print(" filename - name of file you wish to submit for analysis")
    sys.exit(1)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        usage()
    user = getpass.getuser()
    file_to_submit = sys.argv[1]
    print("Enter the SLO password for user {} when prompted".format(user))
    password = getpass.getpass()
    cookies = authenticate(API_URL, user, password)
    if not cookies:
        print("Authentication failed for user {}".format(user))
        sys.exit(1)
    r = submit_file('analysis', file_to_submit, API_URL, cookies)
    r = submit_file('analysis-trace', file_to_submit, API_URL, cookies)
    print(r.text)

