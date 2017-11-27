/***********************************************************************************************************************************
Main
***********************************************************************************************************************************/
#include "perl/exec.h"

int main(int argListSize, char *argList[])
{
    // Execute Perl since nothing is implemented in C (yet)
    perlExec(perlCommand(argListSize, argList));
}
