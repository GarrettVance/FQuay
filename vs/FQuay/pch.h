//                  
//                  
//      Precompiled header file for project FQuay; 
//      ghv 20190424
//                  
//                  


#ifndef PCH_H
#define PCH_H

#include <string>  
#include <algorithm>    //  for trim;
#include <cctype>       //  for trim;
#include <locale>       //  for trim;


struct SDC_SEGMENT
{
    size_t          startPos;
    size_t          length;
    std::string     strContent;
};


struct SDC_PHRASE
{
    std::string     strContent; 
};


enum class SDC_CONSTRAINT_TYPE
{
    CON_PK, 
    CON_UQ, 
    CON_CK, 
    CON_FK
};



#undef SDC_OPTION_CLI_NO_ARGS




#define SDC_OPTION_TABLES_IN_VECTOR




#ifdef __CYGWIN__   //  ghv 20190424
#define _ASSERTE(_COND) \
  static_cast<void>( sizeof((_COND) ? true : false) ) 
#endif





static inline void ltrim(std::string &paddedString) 
{
    paddedString.erase(   //  (using the "[const_iterator, const_iterator)" version);  
        paddedString.begin(), 
        std::find_if(
            paddedString.begin(), 
            paddedString.end(), 
            [](int ch) { return !std::isspace(ch); }
        )
    );
}



static inline void rtrim(std::string &paddedString) 
{

    paddedString.erase(
        std::find_if(
            paddedString.rbegin(), 
            paddedString.rend(), 
            [](int ch) { return !std::isspace(ch); }  
        ).base(), 
        paddedString.end()
    );
}



static inline void trim(std::string &paddedLR) 
{
    ltrim(paddedLR);

    rtrim(paddedLR);
}



static inline std::string ltrim_copy(std::string paddedLeft) 
{
    ltrim(paddedLeft);

    return paddedLeft;
}



static inline std::string rtrim_copy(std::string paddedRight) 
{
    rtrim(paddedRight);

    return paddedRight;
}



static inline std::string trim_copy(std::string paddedLR) 
{
    trim(paddedLR);

    return paddedLR;
}


#endif //PCH_H



