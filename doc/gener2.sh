#!/bin/sh
#
# PDF documents generation
#
# This is using the pandoc tool to generate PDF versions
# of the documents
#
# Guy Turcotte, November 2020
#

echo "New User Guide..."
pandoc NEW\ USER\ GUIDE.md -o NEW\ USER\ GUIDE.pdf "-fmarkdown-implicit_figures -o" --from=markdown -V geometry:margin=.8in -V fontsize:12pt --toc --highlight-style=espresso
