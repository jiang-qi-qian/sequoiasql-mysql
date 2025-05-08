#include <iostream>
#include <stdio.h>
#include "ossVer.hpp"

using namespace std;

int main()
{
   int major = 0 ;
   int minor = 0 ;
   int patch = 0 ;
   int release = 0 ;
   const CHAR *buildTime = NULL ;
   const CHAR *gitVer = NULL ;
   ossGetVersion(&major, &minor, &patch, &release, &buildTime, &gitVer) ;

   char version[128] = {0};
   if (patch > 0)
   {
      sprintf(version, "Version: %d.%d.%d\nRelease: %d\nGit version: %s\n%s",
              major, minor, patch, release, gitVer, buildTime) ;
   }
   else
   {
      sprintf(version, "Version: %d.%d\nRelease: %d\nGit version: %s\n%s",
              major, minor, release, gitVer, buildTime) ;
   }
   cout << version << endl ;

   return 0;
}