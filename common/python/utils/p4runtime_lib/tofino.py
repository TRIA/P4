from .switch import SwitchConnection

class TofinoDeviceConfig(object):
    def __init__(self, tofino_bin_path=None):
        "Builds the device config for Tofino switches"
        self.p4_device_config = None
        with open(tofino_bin_path, "rb") as f:
            self.p4_device_config = f.read()

    def SerializeToString(self):
        return self.p4_device_config

class TofinoSwitchConnection(SwitchConnection):
    def buildDeviceConfig(self, **kwargs):
        return TofinoDeviceConfig(**kwargs)
