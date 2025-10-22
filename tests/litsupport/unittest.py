import lit.formats
import subprocess
import os

class PythonUnitTest(lit.formats.TestFormat):
    """Python unittest
    """
    def __init__(self):
        super().__init__()

    def execute(self, test, litConfig):
        env = dict(os.environ)
        env.update(test.config.environment)
        try:
            result = subprocess.run(
                ["python3", test.getSourcePath()],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
            )
        except Exception as e:
            return lit.Test.FAIL, str(e)

        output = result.stdout.decode() + result.stderr.decode()
        if result.returncode == 0:
            return lit.Test.PASS, output
        else:
            return lit.Test.FAIL, output

