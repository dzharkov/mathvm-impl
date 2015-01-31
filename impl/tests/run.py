#!/usr/bin/python
from os import listdir
import subprocess 
import difflib
import sys

CMD = "./build/" + ("opt" if len(sys.argv) > 3 and sys.argv[3] == 'true' else "debug") + "/mvm " + sys.argv[1]
TEST_DIR = "./tests/" + sys.argv[2]

def run_test(name):
    #print(CMD)
    p = subprocess.Popen(CMD +" " + TEST_DIR + "/" + name + ".mvm", shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    
    expected = open(TEST_DIR + "/" + name + ".expect").readlines()
    res = p.stdout.readlines()

    if res == expected:
        print(name + ": OK")
    else:
        print(name + ": Error")
        print("".join(list(difflib.context_diff(expected, res, fromfile="expected", tofile="got"))))        

for f in listdir(TEST_DIR):
    if ".mvm" in f and f[0] != '.':
        run_test(f[:-4])



