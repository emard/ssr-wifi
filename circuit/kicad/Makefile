all: conversion

project=adapter
directory=plot

back_copper=$(directory)/$(project)-Back-Negative.pdf
# back_solmask=$(directory)/$(project)-Mask_Back.pdf
back_solmask_negative=$(directory)/$(project)-Mask_Back.pdf

front_copper=$(directory)/$(project)-Front-Negative-Mirror.pdf
# front_solmask=$(directory)/$(project)-Mask_Back-Mirror.pdf
front_solmask_negative=$(directory)/$(project)-Mask_Front-Mirror.pdf

back_tile=$(project)-back-tile.pdf
front_tile=$(project)-front-tile.pdf

#this is A4
PAPERSIZE={210mm,279mm}

#trim input file off margins
TRIM=0mm 0mm 0mm 0mm

# ****** options for eagle ******
# Printer: print to file (PDF)
# Paper: custom paper size 45x80 mm
# orientation: portrait
# alignment: center
# area: full
# [ ] mirror
# [ ] rotate
# [x] black
# [x] solid
# margins 0.5 mm

# ******* options for Kicad *******
# Plot format: Postscript
# Output directory: plot/
#
# layers select: Front, Back, Mask_Front, Mask_Back, PCB edges
#
# [ ] plot sheet reference on all layers
# [ ] plot pads on silkscreen
# [x] plot module value on silkscreen
# [x] plot module reference on silkscreen
# [x] plot other module texts on silkscreen
# [ ] plot invisible texts on silkscreen
# [x] do not tent vias
# [ ] mirrored plot
#
# Drill marks: small
# Scaling: 1:1
# Plot mode: Filled
#
# X scale: 1.00000   Y scale: 1.00000
#
# [x] Negative plot
# [ ] Force A4 output
#
# click [Plot]
# and type 'make'
#
# Negative must be set to help sed script which
# inserts postscript commands to replace rgb color
# with its inverse. Such color inversion works correctly
# only when applied to kicad's negative plot

# do not tent vias - leave them as normal pads for
# hand soldering, as we don't have metalized holes


# convert from PS to PDF
%.pdf: %.ps
	ps2pdf $< $@

# mirror left-right the PDF
%-Mirror.pdf: %.pdf
	pdfflip --suffix 'Mirror' --batch $< --outfile $(directory)

# negative black-white the PDF (slow, converts to bitmap)
#%-Negative.pdf: %.pdf
#	convert -density 600 $< -channel RGBA -negate $@

# fast method, works for kicad
%-Negative.ps: %.ps
	sed '/%%EndComments/a\
	/setrgbcolor {\
	1 exch sub 3 1 roll\
	1 exch sub 3 1 roll\
	1 exch sub 3 1 roll\
	setrgbcolor\
	} bind def' $< > $@

conversion: $(back_tile) $(front_tile)

#$(back_solmask_negative): $(back_solmask)
#	convert -density 600 $< -channel RGBA -negate $@


# apt-get install pdfjam
# or 
# apt-get install texlive-extra-utils texlive-latex-recommended
# which also provides pdfjam
# trim left, bottom, right, top
$(back_tile): $(back_copper) $(back_solmask_negative)
	pdfjam --suffix nup \
	--landscape \
	--papersize '{210mm,279mm}' \
	--trim '97mm 76mm 114mm 47mm' \
	--clip 'true' \
	--noautoscale 'true' \
	--scale '1' \
	--no-landscape \
	--nup '2x3' \
	--outfile $@ \
	-- \
	$(back_copper) 1 \
	$(back_solmask_negative) 1,1 \
	$(back_copper) 1,1,1 \

# left and right interchanged
$(front_tile): $(front_copper) $(front_solmask_negative)
	pdfjam --suffix nup \
	--landscape \
	--papersize '{210mm,279mm}' \
	--trim '114mm 76mm 97mm 47mm' \
	--clip 'true' \
	--noautoscale 'true' \
	--scale '1' \
	--no-landscape \
	--nup '2x3' \
	--outfile $@ \
	-- \
	$(front_copper) 1 \
	$(front_solmask_negative) 1,1 \
	$(front_copper) 1,1,1 \

# convert to rich black for printing on
# color cmyk printer
%.pdf.richblack: %.pdf
	gs \
	-o$@ \
	-sDEVICE=pdfwrite \
	-dUseCIEColor \
	-sProcessColorModel=DeviceCMYK \
	-sColorConversionStrategy=CMYK \
	-sColorConversionStrategyForImages=CMYK \
	$<
	
# check color space with this
# identify -verbose

#	pdfnup \
#	--papersize "$(PAPERSIZE)" --trim "$(TRIM)" --clip true \
#	--noautoscale true --scale 1 \
#	--no-landscape \
#	--nup 4x3 \
#	$(back_solmask_negative) \
#	"1,1,1,1,1,1,1,1" \
#	--outfile $@

clean:
	rm -f *~ $(front_tile) $(back_tile) $(back_solmask_negative) $(front_solmask_negative) $(directory)/*.pdf DEADJOE *.[sbl]#[0-9#]
