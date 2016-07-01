import os
import ast
from overrides import overrides

from DataSrc import DataSrc


class StatDataSrc(DataSrc):

    def __init__(self, stat_path):
        self.stat_path = stat_path

    @overrides
    def isAvailable(self):
        return os.path.isfile(self.stat_path)

    @overrides
    def fetch(self):
        with open(self.stat_path, "rt") as fin:
            return ast.literal_eval(fin.read())
