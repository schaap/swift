#define MAXMACHINEPERROLE 256
#define MAXROLEPERTEST 256
#define MAXTEST 2048
#define MAXMACHINE 256
#define MAXCORE 256
#define MAXFILE 1024

struct machine {
    char* name;
    char* address;
    char* user;
    char* params;
    char* tmpdir;
    char* count;
    int icount;
    int coreCount;
    char* cores[MAXCORE];
};

struct role {
    int machineCount;
    struct machine machines[MAXMACHINEPERROLE];
    char* core;
    char* file;
    char* params;
    char* type;
};

struct core {
    char* name;
    char* params;
    char* localdir;
    char* compdir;
    char* program;
};

struct file {
    char* name;
    char* size;
    size_t isize;
    char* offset;
    size_t ioffset;
};

struct test {
    char* name;
    int roleCount;
    struct role roles[MAXROLEPERTEST];
};

int machineCount = 0;
struct machine machines[MAXMACHINE];
int coreCount = 0;
struct core cores[MAXCORE];
int fileCount = 0;
struct file files[MAXFILE];
int testCount = 0;
struct test tests[MAXTEST];

void xmlErrorHandler( void* arg, const char* msg, XmlParserSeverities severity, XmlTextReaderLocatorPtr locator ) {
    printf( "Error in XML config: %s\nSeverity: %i\n", severity );
    exit( -1 );
}

void skipToEndElement( xmlTextReaderPtr xmlRead, const char* name ) {
    while( nodeType != XmlNodeType.EndElement || strcmp( xmlTextReaderConstLocalName( xmlRead ), name ) )
        xmlTextReaderRead( xmLRead );
}

char* readString( xmlTextReaderPtr xmlRead ) {
    char* s = (char*) xmlTextReaderReadInnerXML( xmlRead );
    if( s == NULL || strlen( s ) == 0 ) {
        printf( "Error on line %d: expected string\n", xmlTextReaderGetParserLineNumber( xmlRead ) );
        return NULL;
    }
    return s;
}

char* readRequiredAttribute( xmlTextReaderPtr xmlRead, const char* name ) {
    char* s = (char*) xmlTextReaderGetAttribute( xmlRead, name );
    if( s == NULL || strlen( s ) == 0 ) {
        printf( "Error on line %d: expected required attribute\n", xmlTextReaderGetParserLineNumber( xmlRead ) );
        return NULL;
    }
    return s;
}

#define quit(...) { printf(__VA_ARGS__); exit( -1 ); }
#define onlyOne( var, ent, decl ) { if( var ) quit( "Expected only one '%s' declaration per %s, got second on line %d\n", ent, decl, xmlTextReaderGetParserLineNumber( xmlRead ) ); }
#define readValidString( var, what ) { if( !( var = readString( xmlRead ) ) ) quit( "Invalid %s\n", what ); }
#define readValidRequiredAttribute( var, decl, name ) { if( !( var = readRequiredAttribute( xmlRead, name ) ) ) quit( "Invalid %s declaration: attribute '%s' required\n", decl, name ); }

void readConfig( const char* filename ) {
    /* States:
     * 0 : reading new entities ( to: 1, 3, 4, 5 )
     * 1 : reading machine
     * 2 : reading role
     * 3 : reading core
     * 4 : reading file
     * 5 : reading test ( to: 2 )
     */
    int state = 0;

    struct machine m;
    struct role r;
    struct file f;
    struct core c;
    struct test t;

    xmlTextReaderPtr xmlRead = xmlReaderForFile( filename, NULL, XML_PARSE_NONET | XML_PARSE_NOENT | XML_PARSE_NOCDATA | XML_PARSE_NOXINCNODE | XML_PARSE_COMPACT );
    if( !xmlRead )
        quit( "Could not open %s for reading as XML config\n", filename );

    xmlTextReaderSetErrorHandler( xmlRead, xmlErrorHandler, 0 );

    int nodeType, ret;
    xmlChar* nodeName;
    while( 1 ) {
        if( ( ret = xmlTextReaderRead( xmlRead ) ) != 1 ) {
            if( ret < 0 )
                quit( "Error occurred\n" );
            if( state )
                quit( "No more nodes to read, but state is not 0. Incomplete elements.\n" );
            xmlTextReaderClose( xmlRead );
            return;
        }
        nodeType = xmlTextReaderNodeType( xmlRead );
        
        nodeName = NULL;
        switch( nodeType ) {
            case XmlNodeType.Element :
                nodeName = xmlTextReaderLocalName( xmlRead );
                switch( state ) {
                    case 0 :
                        if( !strcmp( nodeName, "machine" ) ) {
                            state = 1;
                            bzero( m, sizeof( struct machine ) );
                            readValidRequiredAttribute( m.name, "machine", "name" );
                            readValidRequiredAttribute( m.address, "machine", "address" );
                            if( !strcmp( m.address, "DAS4" ) ) {
                                readValidRequiredAttribute( m.count, "machine", "DAS4 node count" );
                                char* end = NULL;
                                m.icount = strtol( m.count, &end, 10 );
                                if( end == m.count || *end || m.icount < 1 )
                                    quit( "Invalid DAS4 node count for machine %s\n", m.name );
                            }
                            break;
                        }
                        if( !strcmp( nodeName, "core" ) ) {
                            state = 3;
                            bzero( c, sizeof( struct core ) );
                            readValidRequiredAttribute( c.name, "core", "name" );
                            break;
                        }
                        if( !strcmp( nodeName, "file" ) ) {
                            state = 4;
                            bzero( f, sizeof( struct file ) );
                            readValidRequiredAttribute( f.name, "file", "name" );
                            readValidRequiredAttribute( f.size, "file", "size" );
                            char* end = NULL;
                            f.isize = strtol( f.size, &end, 10 );
                            if( end == f.size || f.isize < 1 )
                                quit( "Invalid size specifier for file %s\n", f.name );
                            if( *end && *(end+1) )
                                quit( "Invalid size specifier for file %s\n", f.name );
                            if( *end ) {
                                switch( *end ) {
                                    case 'b' :
                                    case 'B' :
                                        break;
                                    case 'k' :
                                    case 'K' :
                                        f.isize *= 1024;
                                        break;
                                    case 'm' :
                                    case 'M' :
                                        f.isize *= 1024 * 1024;
                                        break;
                                    case 'g' :
                                    case 'G' :
                                        f.isize *= 1024 * 1024 * 1024;
                                        break;
                                    case 't' :
                                    case 'T' :
                                        f.isize *= 1024 * 1024 * 1024 * 1024;
                                        break;
                                    default :
                                        quit( "Invalid size specifier for file %s\n", f.name );
                                }
                            }
                            break;
                        }
                        if( !strcmp( nodeName, "test" ) ) {
                            state = 5;
                            bzero( t, sizeof( struct test ) );
                            readValidRequiredAttribute( t.name, "test", "name" );
                            break;
                        }
                        break;
                    case 1 :
                        if( !strcmp( nodeName, "tmpDir" ) ) {
                            onlyOne( m.tmpdir, "machine", "tmpDir" );
                            readValidString( m.tmpdir, "temporary directory location" );
                            skipToEndElement( "tmpDir" );
                            break;
                        }
                        if( !strcmp( nodeName, "params" ) ) {
                            onlyOne( m.params, "machine", "params" );
                            readValidString( m.params, "params" );
                            skipToEndElement( "params" );
                            break;
                        }
                        printf( "Unexpected element %s in machine %s\n", nodeName, m.name );
                        break;
                    case 2 :
                        if( !strcmp( nodeName, "machine" ) ) {
                            if( r.machineCount > 255 )
                                quit( "Maximum of 256 machines per role passed on line %d\n", xmlTextReaderGetParserLineNumber( xmlRead ) );
                            readValid( r.machine[r.machineCount], "machine name" );
                            r.machineCount++;
                            skipToEndElement( "machine" );
                            break;
                        }
                        if( !strcmp( nodeName, "user" ) ) {
                            onlyOne( r.user, "role", "user" );
                            readValidString( r.user, "user" );
                            skipToEndElement( "user" );
                            break;
                        }
                        if( !strcmp( nodeName, "core" ) ) {
                            onlyOne( r.core, "role", "core" );
                            readValidString( r.core, "core name" );
                            skipToEndElement( "core" );
                            break;
                        }
                        if( !strcmp( nodeName, "file" ) ) {
                            onlyOne( r.file, "role", "file" );
                            readValidString( r.file, "file name" );
                            skipToEndElement( "file" );
                            break;
                        }
                        if( !strcmp( nodeName, "params" ) ) {
                            onlyOne( r.params, "role", "params" );
                            readValidString( r.params, "params" );
                            skipToEndElement( "params" );
                            break;
                        }
                        printf( "Unexpected element %s in role on line %d\n", nodeName, xmlTextReaderGetParserLineNumber( xmlRead ) );
                        break;
                    case 3 :
                        if( !strcmp( nodeName, "params" ) ) {
                            onlyOne( c.params, "core", "params" );
                            readValidString( c.params, "params" );
                            skipToEndElement( "params" );
                            break;
                        }
                        if( !strcmp( nodeName, "localDir" ) ) {
                            onlyOne( c.localdir, "core", "localDir" );
                            readValidString( c.localdir, "local core directory" );
                            skipToEndElement( "localDir" );
                            break;
                        }
                        if( !strcmp( nodeName, "compilationDir" ) ) {
                            onlyOne( c.compdir, "core", "compilationDir" );
                            readValidString( c.compdir, "relative compilation directory" );
                            skipToEndElement( "compilationDir" );
                            break;
                        }
                        if( !strcmp( nodeName, "program" ) ) {
                            onlyOne( c.program, "core", "program" );
                            readValidString( c.program, "program name" );
                            skipToEndElement( "program" );
                            break;
                        }
                        printf( "Unexpected element %s in core %s\n", nodeName, c.name );
                        skipToEndElement( nodeName );
                        break;
                    case 4 :
                        if( !strcmp( nodeName, "offset" ) ) {
                            onlyOne( c.offset, "file", "offset" );
                            readValidString( c.offset, "offset" );
                            char* end = NULL;
                            f.ioffset = strtol( f.offset, &end, 10 );
                            if( end == f.offset || *end || f.ioffset < 0 || f >= f.isize )
                                quit( "Invalid file offset for file %s\n", f.name );
                            skipToEndElement( "offset" );
                            break;
                        }
                        printf( "Unexpected element %s in file %s\n", nodeName, f.name );
                        skipToEndElement( nodeName );
                        break;
                    case 5 :
                        if( !strcmp( nodeName, "role" ) ) {
                            state = 2;
                            bzero( r, sizeof( struct role ) );
                            readValidRequiredAttribute( r.type, "role", "type" );
                            if( strcmp( r.type, "seed" ) && strcmp( r.type, "leech" ) )
                                quit( "Invalid value for attribute 'type' for role on line %d, expected 'seed' or 'leech'\n", xmlTextReaderGetParserLineNumber( xmlRead ) );
                            break;
                        }
                        quit( "Unexpected element %s in test on line %d\n", nodeName, xmlTextReaderGetParserLineNumber( xmlRead ) );
                    default :
                        quit( "State sanity\n", nodeName );
                }
                break;
            case XmlNodeType.EndElement :
                nodeName = xmlTextReaderLocalName( xmlRead );
                switch( state ) {
                    case 0 :
                        quit( "End element %s found while not in entity\n", nodeName );
                    case 1 :
                        if( !strcmp( nodeName, "machine" ) ) {
                            memcpy( machines + machineCount, &m, sizeof( struct machine ) );
                            machineCount++;
                            state = 0;
                        }
                        break;
                    case 2 :
                        if( !strcmp( nodeName, "role" ) ) {
                            mempcy( t.roles + r.roleCount, &r, sizeof( struct role ) );
                            t.roleCount++;
                            state = 5;
                        }
                        break;
                    case 3 :
                        if( !strcmp( nodeName, "core" ) ) {
                            memcpy( cores + coreCount, &c, sizeof( struct core ) );
                            coreCount++;
                            state = 0;
                        }
                        break;
                    case 4 :
                        if( !strcmp( nodeName, "file" ) ) {
                            memcpy( files + fileCount, &f, sizeof( struct file ) );
                            fileCount++;
                            state = 0;
                        }
                        break;
                    case 5 :
                        if( !strcmp( nodeName, "test" ) ) {
                            memcpy( tests + testCount, &t, sizeof( struct test ) );
                            testCount++;
                            state = 0;
                        }
                        break;
                    default :
                        quit( "State sanity\n" );
                }
            default :
        }
        if( nodeName )
            free( nodeName );
    }
}

void validateConfigAndGenerateScripts( ) {
    int i, j, k, l;

    // Validate test-role references to machines, cores and files
    // Also register which machine uses which cores
    for( i = 0; i < testCount; i++ ) {
        for( j = 0; j < tests[i].roleCount; j++ ) {
            bool found;
            found = false;
            for( k = 0; k < coreCount; k++ ) {
                if( !strcmp( cores[k].name, tests[i].roles[j].core ) ) {
                    found = true;
                    break;
                }
            }
            if( !found )
                quit( "Test %s role %i refers to core %s which does not exist\n", tests[i].name, j, tests[i].roles[j].core );
            for( l = 0; l < tests[i].roles[j].machineCount; l++ ) {
                found = false;
                for( k = 0; k < machineCount; k++ ) {
                    if( !strcmp( machines[k].name, tests[i].roles[j].machines[l] ) ) {
                        machines[k].cores[machines[k].coreCount++] = tests[i].roles[j].core;
                        found = true;
                        break;
                    }
                }
                if( !found )
                    quit( "Test %s role %i refers to machine %s which does not exist\n", tests[i].name, j, tests[i].roles[j].machines[l] );
            }
            found = false;
            for( k = 0; k < fileCount; k++ ) {
                if( !strcmp( files[k].name, tests[i].roles[j].file ) ) {
                    found = true;
                    break;
                }
            }
            if( !found )
                quit( "Test %s role %i refers to file %s which does not exist\n", tests[i].name, j, tests[i].roles[j].file );
        }
    }

    /*
     * machines
     * cores
     * files
     * tests
     *   roles
     */
    
    // Create temporary script
    FILE* script = tmpfile();
    if( !script )
        quit( "Can't create temporary script\n" );
    fprintf( script, "#!/bin/bash\n" );

    // Check validity of machines and users: can each machine be accessed?
    for( i = 0; i < machineCount; i++ ) {
        fprintf( script, "function ssh_machine_%i {\n", i );
        if( strcmp( machines[i].address, "DAS4" ) )
            fprintf( script, "    ssh -T -n -o BatchMode=yes -h \"%s\"", machines[i].address );
        else
            fprintf( script, "    ssh -T -n -o BatchMode=yes -h fs3.das4.tudelft.nl" );
        if( machines[i].user )
            fprintf( script, " -l \"%s\"", machines[i].user );
        if( machines[i].params )
            fprintf( script, " %s", machines[i].params );
        fprintf( script, " $1 || return -1;\n" );
        fprintf( script, "}\n" );
        fprintf( script, "ssh_machine_%i || exit -1\n", i );
    }

    // Check validity of each core: can each core be packaged? Can it be compiled locally and does the program then exist?
    fprintf( script, "CLEANUPFILE=`mktemp`\n" );
    fprintf( script, "chmod +x CLEANUPFILE\n" );
    fprintf( script, "[ -x CLEANUPFILE ] || exit -1\n" );
    fprintf( script, "function cleanup {\n" );
    fprintf( script, "    (\ncat <<EOL\nrm $CLEANUPFILE\nEOL\n) >> $CLEANUPFILE\n" );
    fprintf( script, "    . $CLEANUPFILE\n" );
    fprintf( script  "    exit $1\n" );
    fprintf( script, "}\n" );
    fprintf( script, "TARDIR=`mktemp -d`\n[ \"X${TARDIR}X\" == \"XX\" ] && exit -1\n" );
    fprintf( script, "(\ncat <<EOL\n#!/bin/bash\nrm -rf $TARDIR\nEOL\n) >> $CLEANUPFILE\n" );
    fprintf( script, "CURDIR=`pwd`\n" );
    for( i = 0; i < coreCount; i++ ) {
        if( !cores[i].localdir )
            cores[i].localdir = strdup( "../" );
        fprintf( script, "cd %s || cleanup -1\n", cores[i].localdir );
        fprintf( script, "make clean\n" ); // Not checked: SHOULD be available, but...
        fprintf( script, "tar cf ${TARDIR}/core_%i.tar . || cleanup -1\n", i );
        fprintf( script, "bzip2 ${TARDIR}/core_%i.tar || cleanup -1\n", i );
        if( !cores[i].compdir )
            cores[i].compdir = strdup( "testenvironment/" );
        fprintf( script, "cd %s || cleanup -1\n", cores[i].compdir );
        fprintf( script, "make || cleanup -1\n" );
        if( !cores[i].program )
            cores[i].program = strdup( "swift" );
        fprintf( script, "[ -x %s ] || cleanup -1\n", cores[i].program );
        fprintf( script, "cd ${CURDIR}\n" );
    }

    // For each machine, copy and compile each core to be used on that machine. Is the core compiled succesfully and does the program exist? Keep track of temp dirs for this.
    for( i = 0; i < machineCount; i++ ) {
        if( machines[i].tmpdir )
            fprintf( script, "REMOTECOREDIR_MACHINE%i=`ssh_machine_%i \"mktemp -d --tmpdir \\\"%s\\\"\" || cleanup -1`\n", i, machines[i].tmpdir );
        else
            fprintf( script, "REMOTECOREDIR_MACHINE%i=`ssh_machine_%i \"mktemp -d\" || cleanup -1`\n", i );
        fprintf( script, "[ \"X${REMOTECOREDIR_MACHINE%i}X\" == \"XX\" ] && cleanup -1\n" );
        fprintf( script, "(\ncat <<EOL\nssh_machine_%i \"rm -rf $REMOTECOREDIR_MACHINE%i\"\nEOL\n) >> $CLEANUPFILE\n", i, i );
        for( j = 0; j < machines[i].coreCount; j++ ) {
            for( k = 0; k < coreCount; k++ ) {
                if( !strcmp( machines[i].cores[j], cores[k] ) ) {
                    l = k;
                    break;
                }
            }
            fprintf( script, "scp -o BatchMode=yes " );
            if( machines[i].params )
                fprintf( script, "%s ", machines[i].params );
            fprintf( script, "$TARDIR/core_%i.tar.bz2 ", l );
            if( machines[i].user )
                fprintf( script, "\"%s\"@", machines[i].user );
            if( strcmp( machines[i].address, "DAS4" ) )
                fprintf( script, "\"%s\"", machines[i].address );
            else
                fprintf( script, "fs3.das4.tudelft.nl" );
            fprintf( script, ":$REMOTECOREDIR_MACHINE%i/core_%i.tar.bz2 || cleanup -1\n", i, l )
            fprintf( script, "ssh_machine_%1$i \"cd $REMOTECOREDIR_MACHINE%1$i || exit -1; mkdir core_%2$i || exit -1; cd core_%2$i || exit -1; tar xjf ../core_%2$i.tar.bz2 || exit -1; cd %3$s || exit -1; make || exit -1; [ -x %4$s ] || exit -1;\" || cleanup -1\n", i, l, cores[l].compdir, cores[l].program );
        }

    // For each file, precalculate the hash
    for( i = 0; i < fileCount; i++ ) {
        // TODO: Build a small tool that allows usage of libswift to precalculate the hashes of files
    }
    // For each test, generate its script
}

/*
struct file {
    char* name;
    char* size;
    size_t isize;
    char* offset;
    size_t ioffset;
};

struct core {
    char* name;
    char* params;
    char* localdir;
    char* compdir;
    char* program;
};

struct machine {
    char* name;
    char* address;
    char* user;
    char* params;
    char* tmpdir;
    char* count;
    int icount;
};

struct role {
    int machineCount;
    struct machine machines[MAXMACHINEPERROLE];
    char* core;
    char* file;
    char* params;
    char* type;
};

struct test {
    char* name;
    int roleCount;
    struct role roles[MAXROLEPERTEST];
};
*/

int main( int argc, char** argv ) {
    machineCount = 0;
    coreCount = 0;
    fileCount = 0;
    testCount = 0;
    bzero( &machines, MAXMACHINE * sizeof( struct machine ) );
    bzero( &cores, MAXCORE * sizeof( struct core ) );
    bzero( &files, MAXFILE * sizeof( struct file ) );
    bzero( &tests, MAXTEST * sizeof( struct test ) );

    if( argc < 2 )
        quit( "Usage: %s configfile\n", argv[0] );

    readConfig( argv[1] );
    validateConfigAndGenerateScripts( );
}
