import os


class LogFile(object):

    def __init__(self, path, file_name):
        self.file_path = os.path.join(path, file_name)
        if not os.path.exists(path):
            os.makedirs(path)

    def recordFunc(self, name, args, result):
        with open(self.file_path, "a") as log_file:
            msg = "{0}(in:{1},out:{2}\n".format(name, str(args), str(result))
            log_file.write(msg)

    def recordOut(self, src, msg):
        with open(self.file_path, "a") as log_file:
            log_file.write(src + ":" + msg + "\n")
