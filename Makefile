.PHONY: all build clean run skelform

all: build

build:
	@cmd /c build_aseprite.bat

run:
	@build\bin\aseprite.exe

clean:
	@if exist build rmdir /s /q build

skelform:
	@cd ..\SkelForm && cargo build --release

skelform-run:
	@cd ..\SkelForm && cargo run
