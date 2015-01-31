#!/usr/bin/python

import subprocess
import sys
import os
import optparse

options = None

def runProg(bin, arg, stdin = None):
    pipe = subprocess.Popen([bin, arg],
                            stdin=subprocess.PIPE, stdout=subprocess.PIPE)
    (out, err) = pipe.communicate()
    return out

def readFile(name):
    f = open(name, 'r')
    result = f.read()
    f.close()
    return result

def writeFile(name, content):
    f = open(name, 'w')
    f.write(content)
    f.close()

def buildOptions():
  result = optparse.OptionParser()
  result.add_option('-e', '--executable',
                    action='store', type='string',
                    help='path to the executable')
  result.add_option('-k', '--kind',
                    action='store', type='string',
                    help='executable kind')
  result.add_option('-t', '--testdir',
                    action='store', type='string',
                    help='tests directory')
  result.add_option('-v', "--verbose",
                    default=False, action='store_true',
                    help='more verbose output')
  result.add_option('-d', '--doublerun',
                    default=False, action='store_true',
                    help='run twice, and compare results')

  return result


diff = '/usr/bin/diff'

def runTest(mvm, root, test, doublerun):
    try:
        expectFile = None
        testFile = os.path.join(root, test+'.mvm')
        if doublerun:
            result1 = runProg(mvm, testFile)
            expectFile = os.path.join('/tmp', 'temp.mvm')
            writeFile(expectFile, result1)
            result = runProg(mvm, expectFile)
        else:
            expectFile = os.path.join(root, test+'.expect')
            result = runProg(mvm, testFile)
        expect = readFile(expectFile)

       
        if expect == result:
            print 'Test "'+test+'" has PASSED'
        else:
            print 'Test "'+test+'" has FAILED'
            print 'Expected: '           
            print '**************************'
            print expect
            print 'Result: '
            print '**************************'
            print result
            print '**************************'
            pipe = subprocess.Popen([diff, '-u', '-b', expectFile, '-'],
                                    stdin=subprocess.PIPE, stdout=subprocess.PIPE)
            (out, err) = pipe.communicate(result)
            print out
    except Exception, e:
        print "Failed to execute the test " + test
        print e

def main(argv):
  global options
  import glob
  import re
    
  parser = buildOptions()
  (options, args) = parser.parse_args()
  kind = options.kind
  if kind is None:
    kind = 'debug'
  testdir = options.testdir
  if testdir is None:
    testdir = './tests'
  mvm = options.executable
  if mvm is None:
    mvm = os.path.join('./build', kind, 'mvm')
  tests = glob.glob(os.path.join(testdir, '*.mvm'))
  for t in tests:
    m = re.search(r"([\w-]+)\.mvm", t)
    if m is None:
      continue
    t = m.group(1)
    runTest(mvm, testdir, t, options.doublerun)

if __name__ == '__main__':
    main(sys.argv)
