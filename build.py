#!/usr/bin/env python3

import os
import sys
import shutil
import argparse
import subprocess

MYSQL_DEFAULT_VERSION = '5.7.44'
MARIADB_DEFAULT_VERSION = '10.4.28'

class OptionsMgr:
    def __init__(self):
        self.__parser = argparse.ArgumentParser()

        # Compile and install options
        build_opt_group = self.__parser.add_argument_group('build arguments')
        build_opt_group.add_argument(
            '--sdbdriver', metavar = 'path', type=str, required=True,
            help='SequoiaDB driver path'
        )

        build_opt_group.add_argument(
            '--commitsha', metavar = 'commitSHA', type=str,
            help='The commit SHA hash value of sequoiasql-mysql repository'
        )

        build_opt_group.add_argument(
            '--builddir', metavar = 'buildDir', type=str,
            help='Path of build directory.',
        )

        build_opt_group.add_argument(
            '--connector', metavar = 'connectorVersion', type=str,
            help='Version number of SequoiaSQL, eg: 3.6-cgb, 3.4.',
        )

        build_opt_group.add_argument(
            '--mysqlsrcpkgdir', metavar = 'mysqlSrcPackageDir',
            type=str, help='The dir of mysql src package.'
        )

        build_opt_group.add_argument(
            '-t', '--type', metavar = 'projectType',
            type=str, default='mysql-' + MYSQL_DEFAULT_VERSION,
            help='Build the project, including the '
                 'testcases. Default: mysql-' + MARIADB_DEFAULT_VERSION)
        build_opt_group.add_argument(
            '-e', '--enterprise',
            default=False,
            help='Build enterprise edition. Default: False',
            action='store_true'
        )
        build_opt_group.add_argument(
            '--hybrid',default=False,
            help='Build hybrid version .Default: False',
            action='store_true'
        )
        build_opt_group.add_argument(
            '--dd', default=False,
            help='Build debug version. Default: False',
            action='store_true'
        )
        build_opt_group.add_argument(
            '-f', '--compileflags', metavar='compileFlags',
            type=str,
            help='Flags for the C/C++ compiler'
        )
        build_opt_group.add_argument(
            '-i', '--install', metavar = 'installPath',
            help='Install the program to the specified path. When \'\' is '
                 'passed, the default value will be used. Default: "install" '
                 'directory in the project root'
        )
        build_opt_group.add_argument(
            '-j', '--jobs', metavar = 'jobNum', type=int, default=2,
            help='Compile thread number. Default: 2' )
        build_opt_group.add_argument(
            '-v', '--verbose',
            help='Print verbose information during compilation. Default: False',
            action='store_true')

        pack_opt_group = self.__parser.add_argument_group('package arguments')
        pack_opt_group.add_argument(
            '-p', '--package',
            help='Make the bin package. Default: False',
            action='store_true')
        pack_opt_group.add_argument(
            '-r', '--runpackage',
            help='Make the run package. Default: False',
            action='store_true'
        )
        pack_opt_group.add_argument(
            '--excludetest', default=False,
            help='Exclude tests from the package. Default: False',
            action='store_true'
        )
        pack_opt_group.add_argument(
            '--archivetest', default=False,
            help='Archive tests into a seperated TGZ file. Default: False',
            action='store_true'
        )

        # Test options
        test_opt_group = self.__parser.add_argument_group('test arguments')
        test_opt_group.add_argument(
            '--test', default=False,
            help='Run all the testcases. Default: False', action='store_true')
        test_opt_group.add_argument(
            '--suite', metavar='names', default='main,json',
            help='Run a suite or a comma separated list of suites. Default: '
                 'main,json'
        )
        test_opt_group.add_argument(
            '--big-test', default=False,
            help='Allow tests marked as "big" to run. Default: False',
            action='store_true'
        )
        test_opt_group.add_argument(
            '--force', default=False,
            help='Continue execution regardless of test case failure. '
                 'Default: False',
            action='store_true'
        )
        test_opt_group.add_argument(
            '--max-test-fail', metavar='N', default=0,
            help='Stop execution after the specified number of tests have '
                 'failed. Default: 0, which means no limit'
        )
        test_opt_group.add_argument(
            '--retry', metavar='N', default=1,
            help='Retry up to a maximum of N runs. Default: 1'
        )
        test_opt_group.add_argument(
            '--retry-failure', metavar='N', default=1,
            help='Allow a failed and retried test to fail more than the '
                 'default times before giving it up'
        )
        test_opt_group.add_argument(
            '--parallel', metavar='N', default=4,
            help='Run tests using N parallel threads. Default: 4'
        )
        test_opt_group.add_argument(
            '--xml-report', metavar='report', default='mysql_test_report.xml',
            help='Generate an xml file containing result of the test run and '
                 'write it to the file named as the option argument'
        )

        self.__parser.add_argument(
            '--clean', default=False,
            help='Clean the total project. Warning: Be carefull to use this '
                 'option, all changes at local will be lost!',
            action='store_true'
        )

    def description(self):
        print("Run options:")
        print("\tBuild      -- " + str(self.needBuild()))
        print("\tBuild jobs -- " + str(self.jobNum()))
        print("\tInstall    -- " + str(self.needInstall()))
        print("\tTest       -- " + str(self.needTest()))
        print("\tPackage    -- " + str(self.needPackage()))
        print("\tRunPackage -- " + str(self.needRunPackage()))

    def parseArguments(self):
        self.args = self.__parser.parse_args()            
        # Parse build type
        target = self.args.type.split('-')
        if len(target) > 2 or len(target) < 1:
            print("The build target '{}' is invalid".format(self.args.type))
            return 1

        if 'mysql' != target[0].lower() and \
            'mariadb' != target[0].lower():
            print("The build target '{}' is invalid".format(self.args.type))
            return 1

        if 'mariadb' == target[0].lower():
            self.__projectType = 'MARIADB'
            self.__projectVersion = MARIADB_DEFAULT_VERSION
        else:
            self.__projectType = 'MYSQL'
            self.__projectVersion = MYSQL_DEFAULT_VERSION
        if len(target) == 2:
            self.__projectVersion = target[1]

        if ((not self.needInstall()) and
           (self.needTest() or self.needPackage() or self.needRunPackage() )):
           print("Installation should be done before testing/packing")
           return 1

        # Change to absolute path to avoid directory switch impact.
        self.args.sdbdriver = os.path.abspath(self.args.sdbdriver)
        if '' == self.args.install:
            prj_dir = os.path.abspath(os.path.dirname(__file__))
            self.args.install = os.path.join(prj_dir, 'output')
        elif self.args.install is not None:
            self.args.install = os.path.abspath(self.args.install)
        return 0

    def getCMakeConfArgList(self):
        cmake_arguments = []
        # Add project type
        cmake_arguments.append('-D{}={}'.format(self.__projectType,
                               self.__projectVersion))
        cmake_arguments.append('-DWITH_SDB_DRIVER={}'
            .format(os.path.join(os.getcwd(), self.args.sdbdriver)))
        if self.args.install is not None:
            cmake_arguments.append(
                '-DCMAKE_INSTALL_PREFIX={}'.format(self.args.install)
            )

        if self.args.commitsha:
            cmake_arguments.append('-DCOMMIT_SHA={}'
                                   .format(self.args.commitsha)
                                  )

        if self.args.connector:
            cmake_arguments.append('-DCONNECTOR={}'
                                   .format(self.args.connector)
                                  )

        if self.args.mysqlsrcpkgdir:
            cmake_arguments.append('-DMYSQL_SRC_PACKAGE_DIR={}'
                                   .format(self.args.mysqlsrcpkgdir))

        if self.args.enterprise:
            cmake_arguments.append('-DENTERPRISE=ON')
        else:
            cmake_arguments.append('-DENTERPRISE=OFF')

        if self.args.hybrid:
            cmake_arguments.append('-DHYBRID=ON')

        if self.args.dd:
            cmake_arguments.append('-DCMAKE_BUILD_TYPE=Debug')

        if self.args.compileflags:
            cmake_arguments.append(self.args.compileflags)

        if self.args.excludetest:
            cmake_arguments.append('-DPACK_TEST=OFF')

        # build without rpath.
        cmake_arguments.append('-DCMAKE_SKIP_RPATH=TRUE')

        print("cmake configuration arguments: {}"
              .format(' '.join(cmake_arguments)))
        return cmake_arguments

    def getTestArgList(self):
        test_arguments = []
        test_arguments.append("--suite={}".format(self.args.suite))
        test_arguments.append("--max-test-fail={}"
                              .format(self.args.max_test_fail))
        test_arguments.append("--retry={}".format(self.args.retry))
        test_arguments.append("--retry-failure={}"
                              .format(self.args.retry_failure))
        test_arguments.append("--parallel={}".format(self.args.parallel))
        test_arguments.append("--xml-report={}".format(self.args.xml_report))

        if self.args.big_test:
            test_arguments.append("--big-test")

        if self.args.force:
            test_arguments.append("--force")

        print("Test arguments: {}".format(' '.join(test_arguments)))
        return test_arguments

    def getProjectType(self):
        return self.__projectType

    def getProjectVersion(self):
        return self.__projectVersion

    def getInstallPath(self):
        return self.args.install

    def needClean(self):
        return self.args.clean

    def needBuild(self):
        return self.args.type

    def needInstall(self):
        return (self.args.install is not None)

    def needTest(self):
        return self.args.test

    def jobNum(self):
        return self.args.jobs

    def needArchiveTest(self):
        return self.args.archivetest

    def needPackage(self):
        return self.args.package

    def needRunPackage(self):
        return self.args.runpackage

class ProjectMgr:
    def __init__(self, optMgr):
        self.__prjRoot = os.path.abspath(os.path.dirname(__file__))
        self.__buildDir = os.path.join(self.__prjRoot, 'build')
        self.__optMgr = optMgr
        print("Project manager created. Root: " + self.__prjRoot)

    def __execute_make_cmd(self, command):
        os.chdir(self.__buildDir)
        print('Execute command: {}'.format(' '.join(command)))
        process = subprocess.Popen(command, shell=False)
        out, error = process.communicate()
        if 0 != process.returncode:
            print('Execute command {} failed'.format(' '.join(command)))
            return 1
        return 0

    def clean_project(self, offline):
        if offline:
            commands = [['rm', '-rf', 'build']]
        else:
            commands = [
                ['git', 'clean', '-fdx'],
                ['git', 'reset', '--hard']
            ]

        for command in commands:
            process = subprocess.Popen(command, shell=False, cwd=self.__prjRoot)
            out, error = process.communicate()
            if 0 != process.returncode:
                raise Exception("Clean project by command '{}' failed: {}"
                                .format(' '.join(command), error))

    def build(self):
        # Set build directory if --builddir is set
        if self.__optMgr.args.builddir:
            self.__buildDir = os.path.join(self.__prjRoot, self.__optMgr.args.builddir)

        try:
            if os.path.exists(self.__buildDir):
                shutil.rmtree(self.__buildDir)
            os.mkdir(self.__buildDir)
        except IOError as err:
            print('IO error: {}'.format(err))
            return 1
        except:
            print('Unexpected error: ', sys.exc_info()[0])
            return 1

        arg_list = self.__optMgr.getCMakeConfArgList()
        build_cmd = ['cmake', '..'] + arg_list

        rc = self.__execute_make_cmd(build_cmd)
        if 0 != rc:
            print("Build project failed, rc: {}".format(rc))
            return rc

        build_cmd =['make', '-j', str(self.__optMgr.jobNum())]
        rc = self.__execute_make_cmd(build_cmd)
        if 0 != rc:
            print("Build project failed, rc: {}".format(rc))
            return rc
        print("Build project successfully!")
        return 0

    def install(self):
        #os.chdir(self.__buildDir)
        install_cmd = ['make', 'install']
        process = subprocess.Popen(install_cmd, shell=False)
        out, error = process.communicate()
        if 0 != process.returncode:
            print("Install program failed: {}".format(error))
            return 1
        return 0

    def archive_test(self):
        pack_command = ['make', 'testpackage']
        rc = self.__execute_make_cmd(pack_command)
        if 0 != rc:
            print("Build test TGZ package for the project failed: {}".format(rc))
            return 1
        print("Build test TGZ package for the project successfully!")
        return 0

    def package(self):
        pack_command = ['make', 'buildpackage']
        rc = self.__execute_make_cmd(pack_command)
        if 0 != rc:
            print("Build TGZ package for the project failed: {}".format(rc))
            return 1
        print("Build TGZ package for the project successfully!")
        return 0

    def buildRunPackage(self):
        pack_command = ['make', 'runpackage']
        rc = self.__execute_make_cmd(pack_command)
        if 0 != rc:
            print("Build .run package for the project failed: {}".format(rc))
            return 1
        print("Build .run package for the project successfully!")
        return 0

    def runTest(self):
        test_args = self.__optMgr.getTestArgList()
        test_dir = os.path.join(self.__optMgr.getInstallPath(), 'mysql-test')
        os.chdir(test_dir)
        exe_path = os.path.join(test_dir, 'mysql-test-run.pl')
        test_cmd = [exe_path] + test_args
        process = subprocess.Popen(test_cmd, shell=False)
        out, error = process.communicate()
        if 0 != process.returncode:
            print("Test failed: {}".format(error))

def main():
    optMgr = OptionsMgr()
    rc = optMgr.parseArguments()
    if 0 != rc:
        print("Resolve arguments failed, rc: {}".format(rc))
        return rc

    optMgr.description()
    prjMgr = ProjectMgr(optMgr)


    if optMgr.needClean():
        offline = False
        if optMgr.args.commitsha:
            offline = True   
        prjMgr.clean_project(offline)

    if optMgr.needBuild():
        rc = prjMgr.build()
        if 0 != rc:
            print("Failed to build the project: {}".format(rc))
            return rc

    if optMgr.needInstall():
        rc = prjMgr.install()
        if 0 != rc:
            print("Install the program failed: {}".format(rc))
            return rc

    if optMgr.needArchiveTest():
        rc = prjMgr.archive_test()
        if 0 != rc:
            print("Make test package failed: {}".format(rc))
            return rc

    if optMgr.needPackage():
        rc = prjMgr.package()
        if 0 != rc:
            print("Make TGZ package for the project failed: {}".format(rc))
            return rc

    if optMgr.needRunPackage():
        rc = prjMgr.buildRunPackage()
        if 0 != rc:
            print("Make .run package for the project failed: {}".format(rc))
            return rc

    if optMgr.needTest():
        rc = prjMgr.runTest()
        if 0 != rc:
            print("Run testcases failed: {}".format(rc))
            return rc

    return 0

if __name__ == "__main__":
    sys.exit(main())
