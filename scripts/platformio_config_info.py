Import("env")


def _define_name(item):
    return item[0] if isinstance(item, tuple) else item


def print_config_info(*_args, **_kwargs):
    defines = [_define_name(item) for item in env.get("CPPDEFINES", [])]
    selected = [name for name in defines if name.startswith("EOLO_TARGET_")]
    features = [name for name in defines if name.startswith("FEATURE_")]
    modes = [name for name in defines if name.startswith("PROFILE_") or name.startswith("STATUS_LED_")]

    print("")
    print("EOLO build configuration")
    print("  Environment:", env.subst("$PIOENV"))
    print("  Profile header: src/Config/Profiles/" + (selected[0].replace("EOLO_TARGET_", "").title().replace("_", "") if selected else "<demo>") + ".h")
    print("  Target:", ", ".join(selected) if selected else "<demo-specific>")
    print("  Features:", ", ".join(features) if features else "<none>")
    print("  Build modes:", ", ".join(modes) if modes else "<default>")
    print("  Tunables: typed values in src/Config/Profiles (not build_flags)")
    print("")


env.AddCustomTarget(
    "config_info",
    None,
    print_config_info,
    title="Config Info",
    description="Show the selected EOLO target, features, and profile header.",
)
