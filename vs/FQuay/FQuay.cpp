//                  
//                  
//              
//              FQuay.cpp : ghv 20190416
//                      
//                          
//      
//                  
//  ASSUMPTION:  the semicolon is a reliable indicator of "end of whole sql statement". 
//      
//      
//      
//  ASSUMPTION:  Within any CREATE TABLE statement, the keyword "CONSTRAINT" is ALWAYS preceded by a comma. 
//      
//      


#include "pch.h"

#include <iostream>
#include <fstream> 
#include <vector> 


#include <regex>  // ghv 20190420; 





static std::string fqfname = "C:\\tmp\\Tesseract.sql";  //  DDL input file;

static std::string targetSchema = "vance";  //  don't clutter the Postgres "public" schema; 

static const int SDC_MAX_FKEYS = 30; // maximum number of foreign key constraints to extract; 

static bool verboseComments = false; 




SDC_CONSTRAINT_TYPE gvGetConstraintType(std::string const& pStr);








bool gvObjectNameIsSchemaQualified(std::string const& pObjectName)
{
    //  ASSUMPTION:  If object name contains a dot then object name is schema-qualified. 

    return  (pObjectName.find(".", 0) != std::string::npos); 
}





std::string gvRemoveSchemaQualifier(std::string const& pStr)
{
    size_t idxDot = pStr.find(".", 0); 

    std::string unqualifiedName = pStr.substr(1 + idxDot); 

    return unqualifiedName;
}





std::string gvSetObjectSchema(std::string const&  rawTableName)
{
    std::string correctedName(rawTableName); 

    if (gvObjectNameIsSchemaQualified(correctedName))
    {
        //  Remove the existing schema name: 

        correctedName = gvRemoveSchemaQualifier(correctedName); 
    }

    //  prepend unqualified name with the new schema name: 

    std::string schemaQualifiedName = targetSchema;
    schemaQualifiedName.append(".");  // append the dot;
    schemaQualifiedName.append(correctedName); 

    return  schemaQualifiedName; 
}






SDC_SEGMENT gvGetTableFromCreateStatement(std::string const&  pSQL, std::regex const& headRegex, std::regex const& tailRegex)
{
    std::smatch headSMatch;
    regex_search(pSQL, headSMatch, headRegex); 

    std::string tailSQL = pSQL.substr(headSMatch.length(0));  // changed length() to length(0);
    std::smatch tailSMatch; 
    regex_search(tailSQL, tailSMatch, tailRegex);  

    std::string trimmedTableName = trim_copy(tailSMatch[0]); 

    SDC_SEGMENT theTable; 

    theTable.length = trimmedTableName.length(); 
    theTable.startPos = headSMatch.length(0);  // changed length() to length(0);
    theTable.strContent = trimmedTableName; 

    return theTable;
}





SDC_SEGMENT gvGetTableFromCreateTable(std::string &  pSQL)
{
    //  std::string headSQL = pSQL; 

    std::regex headRegex("CREATE\\s+TABLE\\s+", std::regex_constants::ECMAScript | std::regex_constants::icase);

    //      
    //  TableName is followed by zero-or-more whitespaces followed by openParenthesis: 
    //  
    
    std::regex tailRegex("\\S+[\\s\\(]", std::regex_constants::ECMAScript | std::regex_constants::icase);

    return gvGetTableFromCreateStatement(pSQL, headRegex, tailRegex); 
}





SDC_SEGMENT gvGetTableFromCreateIndex(std::string const&  pSQL)
{
    //  std::string headSQL = pSQL; 

    std::regex headRegex("CREATE\\s+INDEX\\s+\\S+\\s+ON\\s+", std::regex_constants::ECMAScript | std::regex_constants::icase);

    std::regex tailRegex("\\S+\\s+", std::regex_constants::ECMAScript | std::regex_constants::icase);

    return gvGetTableFromCreateStatement(pSQL, headRegex, tailRegex); 
}




SDC_SEGMENT gvGetTableFromForeignKeyReference(std::string const&  constraintClause)
{
    //  std::string headSQL = constraintClause; 
    //      
    //  CONSTRAINT aldryn_boot_fk_8883 FOREIGN KEY(cmsplugin_ptr_id) REFERENCES cms_cmsplugin(id) DEFERRABLE INITIALLY DEFERRED;
    //      

    std::regex headRegex("CONSTRAINT\\s+\\S+\\s+FOREIGN\\s+KEY\\s+\\S+\\s+REFERENCES\\s+", std::regex_constants::ECMAScript | std::regex_constants::icase);

    //      
    //  The ReferencedTableName is followed by zero-or-more whitespaces followed by openParenthesis: 
    //  

    std::regex tailRegex("\\S+\\s*\\(", std::regex_constants::ECMAScript | std::regex_constants::icase);

    return gvGetTableFromCreateStatement(constraintClause, headRegex, tailRegex); 
}

            









std::string gvIsolateStatement(std::ifstream & rFile, std::string head)
{
    //  Isolate one complete SQL DDL statement by finding the semicolon statement terminator. 
    //      

    std::string streamLine(head); 

    std::string sAccum(head); 

    if (streamLine.find(';', 0) != std::string::npos)  // TODO: merge "if" into "while";
    {
        ; // done;
    }
    else
    {
        while (std::getline(rFile, streamLine))
        {
            sAccum.append(streamLine);

            if (streamLine.find(';', 0) != std::string::npos)
            {
                break; //  If streamLine contains a semicolon, then done.
            }
        }
    }

    return sAccum; 
}










void gvExtractForeignKeys(std::string const& pSQL, std::vector<SDC_PHRASE>&  pManifoldConstraints)
{
    //  cf. gvEraseForeignKeys() method which also uses 
    //  std::string::rfind() to search the SQL statement back-to-front; 
    //      

    //          
	//  Fun Fact: explicit value of std::string::npos is -1, 
    //  by virtue of static constexpr auto npos{static_cast<size_type>(-1)};
    //          

    size_t idxZ = pSQL.find(");", 0); // marks position of the end of this constraint clause;
    size_t idxKW;  // marks position of KEYWORD "CONSTRAINT"; 
    size_t idxY;   // marks position of the COMMA character preceding "CONSTRAINT" keyword;

    for (int jkl = 0; jkl < SDC_MAX_FKEYS; jkl++) // max number of foreign keys to extract; 
    {
        idxKW = pSQL.rfind("CONSTRAINT", idxZ); // TODO: change to regex search;

        if (idxKW == std::string::npos)
        {
            //  If there are no remaining upstream occurrences 
            //  of "CONSTRAINT" keyword, then break out of for loop. 
            //      

            break;
        }

        idxY = pSQL.rfind(",", idxKW);


        // Validate ASSUMPTION that keyword CONSTRAINT is ALWAYS preceded by a comma:  

        _ASSERTE(idxY != std::string::npos);


        //  Now everything between idxY and idxZ comprises one CONSTRAINT clause 
        //  [NOTE however that the interval [idxY, idxZ] includes 
        //  the original leading comma at position idxY. 
        //  While this is important in erasing the FOREIGN KEY clause from CREATE TABLE, 
        //  it is counter-productive for ALTER TABLE ... ADD CONSTRAINT ...
        // 
        //  Thus idxKW is used in place of idxY for ALTER TABLE: 
        //  

        std::string constraintClause = pSQL.substr(idxKW, idxZ - idxKW); //  using idxKW rather than idxY; 

        SDC_CONSTRAINT_TYPE constraintType = gvGetConstraintType(constraintClause);

        if (constraintType == SDC_CONSTRAINT_TYPE::CON_FK)
        {
            if (verboseComments)
            {
                std::cout << "--  ..." << std::endl;
                std::cout << "--  ..." << constraintClause << std::endl;
            }

            constraintClause.push_back(';');  // the terminator;

            //  Deduce table name

            SDC_SEGMENT theTable;

            theTable = gvGetTableFromForeignKeyReference(constraintClause);

            std::string schemaTable = gvSetObjectSchema(theTable.strContent);

            std::string schemaQualifiedFK = constraintClause.replace(theTable.startPos, theTable.length, schemaTable);

            SDC_PHRASE gvStruct;
            gvStruct.strContent = schemaQualifiedFK;
            
            pManifoldConstraints.push_back(gvStruct);

            idxZ = idxY; // update idxZ;
        }
    }
}









bool gvHasForeignKeyClause(std::string const& pStr)
{
    bool retval = false; 

    size_t idxCON1 = pStr.find("CONSTRAINT", 0);  // TODO: regex;

    if (idxCON1 != std::string::npos)
    {
        size_t idxFOREIGN = pStr.find("FOREIGN", idxCON1);
    
        if (idxFOREIGN != std::string::npos)
        {
            //  This SQL string contains a Foreign Key clause, 
            //  i.e. ...CONSTRAINT ... FOREIGN KEY ...

            retval = true;
        }
    }

    return retval;
}






SDC_CONSTRAINT_TYPE gvGetConstraintType(std::string const& pStr) 
{
    SDC_CONSTRAINT_TYPE conType; 
    size_t idxKEYWORD; 

    idxKEYWORD = pStr.find("FOREIGN");

    if (idxKEYWORD != std::string::npos)
    {
        conType = SDC_CONSTRAINT_TYPE::CON_FK;
    }
    else
    {
        idxKEYWORD = pStr.find("PRIMARY");
        
        if (idxKEYWORD != std::string::npos)
        {
            conType = SDC_CONSTRAINT_TYPE::CON_PK;
        }
        else
        {
            idxKEYWORD = pStr.find("CHECK");

            if (idxKEYWORD != std::string::npos)
            {
                conType = SDC_CONSTRAINT_TYPE::CON_CK;
            }
            else
            {
                idxKEYWORD = pStr.find("UNIQUE");
            
                if (idxKEYWORD != std::string::npos)
                {
                    conType = SDC_CONSTRAINT_TYPE::CON_UQ;
                }
                else
                {
                    throw;
                }

            }
        }

    }

    return conType;
}









std::string gvEraseForeignKeys(std::string const& pSQL)
{
    //  This method leverages the std::string::rfind() method 
    //  to search the SQL statement back-to-front; 


    std::string remainderStr = pSQL;  


    size_t idxZ = remainderStr.find(");", 0); // marks position of the end of this constraint clause;
    size_t idxKW;  // marks position of KEYWORD "CONSTRAINT"; 
    size_t idxY;   // marks position of the COMMA character preceding "CONSTRAINT" keyword;


    for (int jkl = 0; jkl < SDC_MAX_FKEYS; jkl++) // max number of foreign keys to extract; 
    {
        idxKW = remainderStr.rfind("CONSTRAINT", idxZ); // TODO: regex;

        if (idxKW == std::string::npos)
        {
            //  If there are no remaining upstream occurrences 
            //  of "CONSTRAINT" keyword, then break out of for loop. 
            //      

            break;
        }

        idxY = remainderStr.rfind(",", idxKW);

        // Validate ASSUMPTION that keyword CONSTRAINT is ALWAYS preceded by a comma:  

        _ASSERTE(idxY != std::string::npos);  

        //  Now everything between idxY and idxZ comprises one CONSTRAINT clause: 

        std::string constraintClause = remainderStr.substr(idxY, idxZ - idxY);  

        SDC_CONSTRAINT_TYPE constraintType = gvGetConstraintType(constraintClause);  

        if (constraintType == SDC_CONSTRAINT_TYPE::CON_FK)
        {
            if (verboseComments)
            {
                std::cout << "--  ..." << std::endl;
                std::cout << "--  ..." << constraintClause << std::endl;
            }

            remainderStr.erase(idxY, idxZ - idxY);  // remove entire FK clause; 

            idxZ = idxY; // update idxZ;
        }
    }

    return remainderStr; 
}











void gvSpoolForeignKeys(std::ifstream& inputFile)
{
    std::string streamLine;

    while (std::getline(inputFile, streamLine)) 
    {
        if (0 == streamLine.compare(0, 12, "CREATE TABLE"))
        {
            //  if streamLine begins "CREATE TABLE..." 
            //  then this is the start of a new SQLStatement. 

            std::string entireSQL = gvIsolateStatement(inputFile, streamLine);  

            //  Deduce table name

            SDC_SEGMENT theTable = gvGetTableFromCreateTable(entireSQL);

            std::string schemaTable = gvSetObjectSchema(theTable.strContent);

            //  Test for presence of FOREIGN KEY clause: 

            if (gvHasForeignKeyClause(entireSQL))
            {
                std::vector<SDC_PHRASE>  vecManifold;

                gvExtractForeignKeys(entireSQL, vecManifold);

                std::vector<SDC_PHRASE>::iterator fkIter;

                for (fkIter = vecManifold.begin(); fkIter != vecManifold.end(); fkIter++)
                {
                    std::string trimmedVersion = (*fkIter).strContent;

                    std::cout << "     " << std::endl;

                    std::cout << "ALTER TABLE " << schemaTable << " ADD " << trimmedVersion << std::endl;
                }
            }
        }
    }
}











void gvSpoolTables(std::ifstream& inputFile)
{
    std::string streamLine;

    while (std::getline(inputFile, streamLine)) 
    {
        if (0 == streamLine.compare(0, 12, "CREATE TABLE")) //  TODO: replace with regex search;
        {
            //  if streamLine begins "CREATE TABLE..." 
            //  then this is the head of a new SQL statement. 

            std::string entireSQL = gvIsolateStatement(inputFile, streamLine);  

            //  Deduce table name

            SDC_SEGMENT theTable;

            theTable = gvGetTableFromCreateTable(entireSQL);

            std::string schemaTable = gvSetObjectSchema(theTable.strContent);

            std::string trimmedCreateTable = entireSQL.replace(theTable.startPos, theTable.length, schemaTable); 

            //  Test for presence of FOREIGN KEY clause: 

            if (gvHasForeignKeyClause(trimmedCreateTable))
            {
                //  Remove all FOREIGN KEY clauses: 

                trimmedCreateTable = gvEraseForeignKeys(trimmedCreateTable);
            }

            std::cout << "--  " << std::endl;
            std::cout << "--  " << std::endl;

            std::cout << trimmedCreateTable << std::endl;
        }
    }
}










void gvSpoolIndexes(std::ifstream& inputFile)
{
    std::string streamLine;

    while (std::getline(inputFile, streamLine)) 
    {
        std::regex kwSkw("CREATE\\s+INDEX", std::regex_constants::ECMAScript | std::regex_constants::icase);

        std::smatch sm; 

        bool retval = regex_search(streamLine, sm, kwSkw);  

        if (retval)
        {
            //  if streamLine begins "CREATE INDEX..." 
            //  then this is the head of a new SQL statement. 

            std::string entireSQL = gvIsolateStatement(inputFile, streamLine);  
            
            //  Deduce table name
        
            SDC_SEGMENT indexTable;

            indexTable = gvGetTableFromCreateIndex(entireSQL);  

            std::string schemaTable = gvSetObjectSchema(indexTable.strContent);  

            std::string schemaCreateIndex = entireSQL.replace(indexTable.startPos, indexTable.length, schemaTable); 

            std::cout << "--  " << std::endl;
            std::cout << "--  " << std::endl;

            std::cout << schemaCreateIndex << std::endl;
        }
    }
}













int main(int argc, char *argv[])
{
    std::vector<std::string> vecArgs; 

#ifdef SDC_OPTION_CLI_NO_ARGS

    verboseComments = false; 

    targetSchema = "vance";  // schema in which to create objects;

    fqfname = "c:\\tmp\\oem.sql";  // fully-qualified filename;

    std::ifstream ddlInputFile; // Namely the Tesseract.sql script;

    ddlInputFile.open(fqfname.c_str());

    if (ddlInputFile.is_open())
    {
        gvSpoolForeignKeys(ddlInputFile); 

        ddlInputFile.close();
    }

    std::cerr << "=======================================================" << std::endl;
    std::cerr << "|                                                     |" << std::endl;
    std::cerr << "|       Executed using SDC_OPTION_CLI_NO_ARGS         |" << std::endl;
    std::cerr << "|                                                     |" << std::endl;
    std::cerr << "=======================================================" << std::endl;


#else

    for (int idxArg = 0; idxArg < argc; idxArg++)
    {
        vecArgs.push_back(std::string(argv[idxArg])); 
    }


    if (argc < 4)
    {
        std::cout << "Usage:" << std::endl;

        std::cout << "DOS> fquay   schema   mode   input_file_name   >   redirected_output_file" << std::endl;
        std::cout << "where mode in {tables, fkeys or indexes}. " << std::endl;
        std::cout << " " << std::endl;

        std::cout << "Examples:" << std::endl;


        std::cout << "DOS> fquay  public  tables    C:\\tmp\\Tesseract.sql  >  create_tables.sql" << std::endl;
        std::cout << "DOS> fquay  public  fkeys     C:\\tmp\\Tesseract.sql  >  create_foreign_keys.sql" << std::endl;
        std::cout << "DOS> fquay  public  indexes   C:\\tmp\\Tesseract.sql  >  create_indexes.sql" << std::endl;
        std::cout << " " << std::endl;

        return -1;  // Usage; 
    }

    verboseComments = false; 

    targetSchema = vecArgs.at(1);  // schema in which to create objects;

    fqfname = vecArgs.at(3);  // fully-qualified filename;

    std::ifstream ddlInputFile; // Namely the Tesseract.sql script;

    ddlInputFile.open(fqfname.c_str());

    if (ddlInputFile.is_open())
    {
        if (0 == vecArgs.at(2).compare("fkeys"))
        {
            gvSpoolForeignKeys(ddlInputFile);
        }
        else if (0 == vecArgs.at(2).compare("tables"))
        {
            gvSpoolTables(ddlInputFile);
        }
        else if (0 == vecArgs.at(2).compare("indexes"))
        {
            gvSpoolIndexes(ddlInputFile);
        }

        ddlInputFile.close();
    }

#endif

    return 0;
}




