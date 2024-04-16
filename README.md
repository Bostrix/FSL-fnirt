# Instructions for Compiling and Running FNIRT
## Introduction
Welcome to Fnirt, a powerful tool designed for non-linear registration of neuroimaging data.
This guide will demonstrate how to effectively utilize Fnirt for precise non-linear registration, ensuring seamless alignment and accurate analysis across brain images.

For more information about Fnirt and related tools, visit the FMRIB Software Library (FSL) website:[FSL Git Repository](https://git.fmrib.ox.ac.uk/fsl) 
and you can also find additional resources and documentation on fnirt on the FSL wiki page: [FNIRT Documentation](https://fsl.fmrib.ox.ac.uk/fsl/fslwiki/FNIRT).

## Clone the Repository

Begin by cloning the project repository from GitHub onto your local machine. You can do this by running the following command in your terminal or command prompt:

```bash
git clone https://github.com/Bostrix/FSL-fnirt.git
```
This command will create a local copy of the project in a directory named "FSL-fnirt".

## Navigate to Project Directory
Change your current directory to the newly cloned project directory using the following command:
```bash
cd FSL-fnirt
```
## Installing Dependencies
To install the necessary dependencies for compiling and building the project, follow these steps:
```bash
sudo apt-get update
sudo apt install g++
sudo apt install make
sudo apt-get install libboost-all-dev
sudo apt-get install libblas-dev libblas3
sudo apt-get install liblapack-dev liblapack3
sudo apt-get install zlib1g zlib1g-dev
```

## Compilation:
To compile fnirt, follow these steps:
- Ensure correct path in Makefile: To ensure that the necessary libraries are included during compilation, please verify that the correct path is specified in the makefile.
 If you are using libraries such as warpfns, basisfield, meshclass, miscmaths, and znzlib, ensure that `$(ADDED_LDFLAGS)` are included in the compile step of the makefile. 
- Confirm that the file `point_list.h` within the warpfns library accurately includes the path to `armawrap/newmat.h`.

- Verify the accurate paths in meshclass's Makefile:
Ensure the correct linker paths for the newimage, miscmaths, NewNifti, znzlib, cprob and utils libraries are included.

- Compiling: 
Execute the appropriate compile command to build the fnirt tool.
```bash
make clean
make
```
- Resolving Shared Library Errors
When running an executable on Linux, you may encounter errors related to missing shared libraries.This typically manifests as messages like:
```bash
./fnirt: error while loading shared libraries: libexample.so: cannot open shared object file:No such file or directory
```
To resolve these errors,Pay attention to the names of the missing libraries mentioned in the error message.Locate the missing libraries on your system.
If they are not present, you may need to install the corresponding packages.If the libraries are installed in custom directories, you need to specify those directories using the `LD_LIBRARY_PATH` environment variable. For example:
```bash
export LD_LIBRARY_PATH=/path/to/custom/libraries:$LD_LIBRARY_PATH
```
Replace `/path/to/custom/libraries` with the actual path to the directory containing the missing libraries.
Once the `LD_LIBRARY_PATH` is set, attempt to run the executable again.If you encounter additional missing library errors, repeat steps until all dependencies are resolved.

- Resolving "The environment variable FSLOUTPUTTYPE is not defined" errors
If you encounter an error related to the FSLOUTPUTTYPE environment variable not being set.Setting it to `NIFTI_GZ` is a correct solution, as it defines the output format for FSL tools to NIFTI compressed with gzip.Here's how you can resolve:
```bash
export FSLOUTPUTTYPE=NIFTI_GZ
```
By running this command, you've set the `FSLOUTPUTTYPE` environment variable to `NIFTI_GZ`,which should resolve the error you encountered.
## Running fnirt

After successfully compiling, you can run fnirt by executing:
```bash
./fnirt --ref=<some template> --in=<some image>
```
This command will execute the fnirt tool.


Customize FNIRT's behavior by specifying additional options as required. Consult the usage guide available in the documentation for a comprehensive list of available options and their descriptions.
