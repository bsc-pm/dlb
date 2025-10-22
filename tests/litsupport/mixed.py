import lit.formats
import litsupport.bets
import litsupport.unittest
import os

class MixedTest(lit.formats.TestFormat):
    def __init__(self):
        self.bets = litsupport.bets.BetsTest()
        self.py = litsupport.unittest.PythonUnitTest()

    def getTestsInDirectory(self, testSuite, path_in_suite, litConfig, localConfig):
        return self.bets.getTestsInDirectory(testSuite, path_in_suite, litConfig, localConfig)

    def execute(self, test, litConfig):
        if test.getSourcePath().endswith(".py"):
            dlb_bindings_dir = os.path.join(os.getcwd(), '..', 'src', 'apis', 'python_bindings')
            test.config.environment['PYTHONPATH'] = os.pathsep.join([dlb_bindings_dir, os.environ.get('PYTHONPATH', '')])
            test.config.environment["DLB_LIB_DIR"] = os.path.join(test.config.test_exec_root, "..", ".libs")
            return self.py.execute(test, litConfig)
        else:
            return self.bets.execute(test, litConfig)
