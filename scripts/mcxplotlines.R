
#  This script takes 5 parameters, described below.
#
#   * data.raw    contains expression/fold-change values (argument to mcxarray -data option).
#
#   * data.tab    contains 'tab file' (created with mcxarray).
#
#  These will generally be related by the following mcxarray invocation:
#  mcxarray -data data.raw -write-tab data.tab -skipc 1 -skipr 1 -o data.mci
#  The file data.mci is not needed by this script, but necessarily exists under
#  some name. Further,
#
#   * out.data.mci.I20   some mcl output file resulting from clustering data.mci.
#                 It might have been created by 'mcl data.mci -I 2'.
#
#   * 15          the number of (largest) clusters you want to visualize.
#
#   * out.pdf     the name of the PDF file you want to create
#
#  Now issue on the command line:
#
#  R --vanilla --args data.raw data.tab out.data.mci.I20 15 out.pdf < mcxplotarray.R
#  Now out.pdf should contain a visualization of the expression values in your clusters.

args <- commandArgs(trailingOnly=TRUE)

if (length(args) != 5) {
   cat("I Need five arguments: \"rawfile\" \"tabfile\" \"clsfile\" \"NUM\" \"pdffile\"\n")
   cat("These should be related by an mcxarray invocation,\n")
   cat("  mcxarray -data \"rawfile\" -write-tab \"tabfile\" -skipc 1 -skipr 1 -o \"mclfile\"\n")
   cat("And an mcl invocation,\n")
   cat("  mcl \"mclfile\" -I 3 -o \"clsfile\"\n")
   cat("\nExample invocation (data.raw, data.tab, out.data.mci.I20 should exist):\n")
   cat("R --slave --vanilla --args data.raw data.tab out.data.mci.I20 20 out.pdf < mcxplotarray.R\n")
   quit(save="no")
}

rawfile <- args[1]
tabfile <- args[2]
clsfile <- args[3]
num     <- as.integer(args[4])
pdffile <- args[5]

#library("limma")
#library("Biobase")

   ## copied from the limma package.
   ##
plotlines <- function (x, first.column.origin = FALSE, xlab = "Column", ylab = "x",
    col = "black", lwd = 1, ...)
{
    x <- as.matrix(x)
    ntime <- ncol(x)
    ngenes <- nrow(x)
    if (first.column.origin)
        x <- x - array(x[, 1], dim(x))
    time <- matrix(1:ntime, ngenes, ntime, byrow = TRUE)
    plot(time, x, type = "n", xlab = xlab, ylab = ylab, ...)
    col <- rep(col, ngenes)
    for (i in 1:ngenes) lines(1:ntime, x[i, ], col = col[i], lwd = lwd)
}


mytbl <- read.table(rawfile)
cat("read rawfile\n")

command <- paste("mcxdump --no-values --transpose -imx",
                     clsfile, "-tabr", tabfile, "| sort -nk 2")
cat(paste("running", "[", command, "]\n"))
fobj <- pipe(command, "r")

cat("reading table ...\n")
cls <- read.table(fobj, header=FALSE, row.names=1,
         colClasses = c('character', 'factor'), check.names=TRUE)
close(fobj)
cat("done reading table\n")

clusters <- tapply(rownames(cls), cls$V2, function(x) { x })

if (num > length(clusters)) {
   num <- length(clusters)
   cat("Not as many clusters are present: adjusting number\n")
}

mymin <- min(mytbl)
mymax <- max(mytbl)

pdf(file=pdffile, width=8.26389,  height=11.6944)
par(mfrow=c(4,3))


for (i in 0:as.integer(num)) {
   cat(paste("cls", i, "\n"))
   subExprs <- mytbl[clusters[[paste(i)]],]
   # subExprs <- mytbl[rownames(cls)[cls[,1] == i],]
   plotlines(subExprs, col='orange',
               main=paste("Cluster ",i,", ", nrow(subExprs), " elements", sep=""),
               axes=FALSE,
               ylim=c(mymin,mymax), xlab="", ylab=""
               )
   # axis(1, at=c(1:ncol(subExprs)), labels=colnames(mytbl), las=3)
   axis(1, at=c(1:ncol(subExprs)), labels=gsub(".WT", "", colnames(mytbl)), las=3)
   axis(2)
}

dev.off()
cat(paste("Output is in", pdffile, "\n"))

