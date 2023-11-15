import logging
import logging.handlers
import os
import sys
from pathlib import Path

import colorlog


class Logger(object):
    """The logger factory class. It is a template to help quickly create a log
    utility.
    Attributes:
    set_conf(log_file, use_stdout, log_level): This is a static method that returns a
    configured logger.
    get_logger(tag): This is a static method that returns a configured logger.
    """

    __loggers = {}

    __use_stdout = True
    __log_file = None
    __log_level = logging.DEBUG

    @staticmethod
    def config(log_file, use_stdout, log_level):
        """Set the config, where config is a ConfigParser object"""
        Logger.__use_stdout = use_stdout
        Logger.__log_level = log_level

        if log_file is not None:
            dirname = os.path.dirname(log_file)
            if (not os.path.isfile(log_file)) and (not os.path.isdir(dirname)):
                try:
                    Path(dirname).mkdir(parents=True)
                except OSError as e:
                    print(f"create path {dirname} for logging failed: {e}")
                    sys.exit()
            Logger.__log_file = log_file

    @staticmethod
    def get_logger(tag):
        """Return the configured logger object"""
        if tag not in Logger.__loggers:
            Logger.__loggers[tag] = logging.getLogger(tag)
            Logger.__loggers[tag].setLevel(Logger.__log_level)
            formatter = colorlog.ColoredFormatter(
                "%(log_color)s[%(name)s][%(levelname)s] %(asctime)s "
                "%(filename)s:%(lineno)s %(message)s",
                log_colors={
                    "DEBUG": "cyan",
                    "INFO": "green",
                    "WARNING": "yellow",
                    "ERROR": "red",
                    "CRITICAL": "bold_red",
                },
            )
            if Logger.__log_file is not None:
                file_handler = logging.handlers.TimedRotatingFileHandler(
                    Logger.__log_file, when="H", interval=1, backupCount=0
                )
                file_handler.setLevel(Logger.__log_level)
                file_handler.setFormatter(formatter)
                file_handler.suffix = "%Y%m%d%H%M.log"
                Logger.__loggers[tag].addHandler(file_handler)
            if Logger.__use_stdout:
                stream_headler = logging.StreamHandler()
                stream_headler.setLevel(Logger.__log_level)
                stream_headler.setFormatter(formatter)
                Logger.__loggers[tag].addHandler(stream_headler)
        return Logger.__loggers[tag]
