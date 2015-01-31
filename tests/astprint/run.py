#!/usr/bin/python
from optparse import OptionParser
import re, os, sys, subprocess

def read_file(name):
    f = open(name, 'r')
    result = f.read()
    f.close()
    return result

def run_test(test_name, executable, test_input):
    result1 = test_name + '_tmp1.mvm'
    with open(result1, 'w') as f:
        subprocess.call([executable, test_input], stdout=f)
    result2 = test_name + '_tmp2.mvm'
    with open(result2, 'w') as f:
        subprocess.call([executable, result1], stdout=f)
    src1 = read_file(result1)
    src2 = read_file(result2)
    if src1 == src2:
        print 'test ' + test_name + ' passed'
        os.remove(result1)
        os.remove(result2)
    else:
        print 'test ' + test_name + ' failed (compare files ' + result1 + ' and ' + result2 + ')'

def args_parser():
    parser = OptionParser()
    parser.add_option('-e', '--executable', action='store'
                          , type='string', help='path to executable')
    parser.add_option('-d', '--testdir', action='store'
                          , type='string', help='path to test .mvm files')
    return parser

def main(argv):
    parser = args_parser()
    (options, args) = parser.parse_args()
    executable = options.executable
    if not executable:
        print 'executable for testing expected (-e)'
        return
    testdir = options.testdir
    if not testdir:
        # refer to parent 'test' dir with mvm files
        testdir = '..'
    elif not os.path.isdir(testdir):
        print testdir, ' is not existing directory'
        return
    for name in os.listdir(testdir):
        match = re.match(r'(.*)\.mvm$', name)
        full_name = os.path.join(testdir, name)
        if match and os.path.isfile(full_name):
            run_test(match.group(1), executable, full_name)    

if __name__ == '__main__':
    main(sys.argv)
