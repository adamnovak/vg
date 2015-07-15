#!/usr/bin/env bash
set -e

# This script downloads the 1000 genomes VCFs and the reference genome, and
# turns them into a vg graph. It takes a lot of CPU, memory, network, and disk
# space. It measures just how much CPU and memory it uses with GNU time.

# We're going to download 1000 genomes' VCFs and its reference (GRCh37) in single-chromosome FASTAs.
# They have chromosome FASTAs identified by IDs "1", "2", "X", etc.

# Function to download 1000 genomes VCFs per chromosome
function get_1000g_vcf {
    # This is the contig name we want: "1", "2", "Y"
    CONTIG=$1
    
    # What's the 1000g base url?
    BASE_URL="ftp://ftp.1000genomes.ebi.ac.uk/vol1/ftp/release/20130502"
    
    if [ "$CONTIG" == "Y" ]
    then
        # chrY has a special filename
        VCF_URL="${BASE_URL}/ALL.chr${CONTIG}.phase3_integrated_v1a.20130502.genotypes.vcf.gz"
    else
        VCF_URL="${BASE_URL}/ALL.chr${CONTIG}.phase3_shapeit2_mvncall_integrated_v5a.20130502.genotypes.vcf.gz"
    fi
    
    # Where do we save it?
    OUTPUT_FILE="vcf/${CONTIG}.vcf.gz"
    
    if [ -e ${OUTPUT_FILE} ]
    then
        # Don't get the same file twice.
        echo "Skipping download of ${VCF_URL}"
    else
        curl -o "${OUTPUT_FILE}" "${VCF_URL}"
        # Get the index too
        curl -o "${OUTPUT_FILE}.tbi" "${VCF_URL}.tbi"
    fi
}

# Function to download GRCh37 reference FASTAs per chromosome
function get_GRCh37_fasta {
    # This is the contig name we want: "1", "2", "Y"
    CONTIG=$1
    
    # This is the URL to get it from
    FASTA_URL="http://hgdownload.cse.ucsc.edu/goldenPath/hg19/chromosomes/chr${CONTIG}.fa.gz"
    
    # And the place to put it
    OUTPUT_FILE="fa/${CONTIG}.fa"
    
     if [ -e ${OUTPUT_FILE} ]
    then
        # Don't get the same file twice.
        echo "Skipping download of ${FASTA_URL}"
    else
        # We're going to download the FASTA, but we need to strip the "chr" from the record names.
        curl "${FASTA_URL}" | gzip -cd | sed "s/chr${CONTIG}/${CONTIG}/" > ${OUTPUT_FILE}
    fi
}

if [ ! -e $0 ]
then
    echo "Please run from the script's directory."
    exit 1
fi

# Make the input file directories
mkdir -p vcf
mkdir -p fa

# Loop over every chromosome
for CONTIG in {1..22} X Y
do


    # Get the VCF and FASTA
    get_1000g_vcf ${CONTIG}
    get_GCRh37_fasta ${CONTIG}
    
    # Make a directory for all the VG files
    mkdir -p vg_parts
    
    # Build the vg graph for this contig
    echo "Building VG graph for chromosome ${CONTIG}"
    time -v ../../vg construct -r "fa/${CONTIG}.fa" -v "vcf/${CONTIG}.vcf.gz" -R "${CONTIG}" -p > "vg_parts/${CONTIG}.vg"
done

# Now we put them in the same ID space
echo "Re-numbering nodes to joint ID space..."
time -v ../../vg ids -j vg_parts/*.vg

# Now we concatenate all the parts into one file
echo "Collecting subgraphs..."
cat vg_parts/*.vg > 1kgp.vg

# And get some stats
echo "Calculating statistics..."
time -v ../../vg stats -zls 1kgp.vg



