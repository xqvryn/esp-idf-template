"""Unversioned NVS settings survive a QEMU reboot and migrate in memory."""


def test_unversioned_settings_migrate_across_reboot(espkit):
    espkit.send("espkit nvs-unversioned", "espkit: nvs-written")
    espkit.dut.write("espkit restart")
    espkit.dut.expect("config: loaded NVS settings outcome=1", timeout=40)
    espkit.dut.expect("espkit: nvs ssid=qemu-net", timeout=10)
    espkit.dut.expect("espkit: ready", timeout=10)
