PYTHON ?= python3
.PHONY: test simulator generate tools console
test:
	$(PYTHON) unit_tests/run.py
simulator:
	$(PYTHON) tools/build_simulator.py
generate:
	$(PYTHON) firmware/tunerstudio/generate.py
tools:
	./gradlew :java_tools:installDist
console:
	./gradlew :java_console:run
