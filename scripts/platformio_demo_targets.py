Import("env")


demo_name = env.GetProjectOption("custom_demo_name", "")
demo_model = env.GetProjectOption("custom_demo_model", "")
demo_src = env.GetProjectOption("custom_demo_src", "")
pioenv = env.subst("$PIOENV")


def print_demo_info(*_args, **_kwargs):
    print("")
    print(f"Demo environment : {pioenv}")
    print(f"Demo             : {demo_name}")
    print(f"Model            : {demo_model}")
    print(f"Source           : {demo_src}")
    print("")
    print("Commands:")
    print(f"  pio run -e {pioenv}")
    print(f"  pio run -e {pioenv} -t upload")
    print(f"  pio device monitor -e {pioenv}")
    print("")


env.AddCustomTarget(
    "demo_info",
    None,
    print_demo_info,
    title="Demo Info",
    description="Show source, model, and commands for this demo environment.",
)
