from pathlib import Path

Import("env")


project_dir = Path(env.subst("$PROJECT_DIR"))
demo_src = env.GetProjectOption("custom_demo_src", "")

if not demo_src:
    raise RuntimeError("custom_demo_src is required for demo environments")

source = project_dir / demo_src
if not source.exists():
    raise RuntimeError(f"Demo source not found: {source}")

env.Append(CPPPATH=[
    str(source.parent),
    str(source.parent.parent),
])

if source.suffix.lower() == ".ino":
    generated_sources_dir = project_dir / ".pio" / "generated_demo_sources" / env.subst("$PIOENV")
    generated_sources_dir.mkdir(parents=True, exist_ok=True)

    wrapper = generated_sources_dir / f"{source.stem}.cpp"
    include_path = source.resolve().as_posix().replace('"', '\\"')
    library_includes = "\n".join(
        line.strip()
        for line in source.read_text(encoding="utf-8").splitlines()
        if line.lstrip().startswith("#include <")
    )
    wrapper.write_text(
        f'#include <Arduino.h>\n{library_includes}\n#include "{include_path}"\n',
        encoding="utf-8",
    )
    build_dir = generated_sources_dir
    build_filter = [f"+<{wrapper.name}>"]
else:
    build_dir = source.parent
    build_filter = [f"+<{source.name}>"]

env.Replace(
    PROJECT_SRC_DIR=str(build_dir),
    SRC_FILTER=build_filter,
)
