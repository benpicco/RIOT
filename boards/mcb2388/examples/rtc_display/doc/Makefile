all: Readme.pdf

%.pdf: %.md
	pandoc --filter pandoc-include-code $< -o $@
