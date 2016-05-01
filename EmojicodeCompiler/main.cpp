//
//  main.c
//  EmojicodeCompiler
//
//  Created by Theo Weidmann on 27.02.15.
//  Copyright (c) 2015 Theo Weidmann. All rights reserved.
//

#include <libgen.h>
#include <getopt.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include "utf8.h"
#include "StaticAnalyzer.hpp"
#include "Class.hpp"
#include "EmojicodeCompiler.hpp"
#include "CompilerErrorException.hpp"

StartingFlag startingFlag;
bool foundStartingFlag;

std::vector<Class *> classes;

char* EmojicodeString::utf8CString() const {
    // Size needed for UTF8 representation
    size_t ds = u8_codingsize(c_str(), size());
    char *utf8str = new char[ds + 1];
    size_t written = u8_toutf8(utf8str, ds, c_str(), size());
    utf8str[written] = 0;
    return utf8str;
}

static bool outputJSON = false;
static bool printedErrorOrWarning = false;
static bool hasError = false;

void printJSONStringToFile(const char *string, FILE *f) {
    char c;
    fputc('"', f);
    while ((c = *(string++))) {
        switch (c) {
            case '\\':
                fprintf(f, "\\\\");
                break;
            case '"':
                fprintf(f, "\\\"");
                break;
            case '/':
                fprintf(f, "\\/");
                break;
            case '\b':
                fprintf(f, "\\b");
                break;
            case '\f':
                fprintf(f, "\\f");
                break;
            case '\n':
                fprintf(f, "\\n");
                break;
            case '\r':
                fprintf(f, "\\r");
                break;
            case '\t':
                fprintf(f, "\\t");
                break;
            default:
                fputc(c, f);
        }
    }
    fputc('"', f);
}

//MARK: Warnings

void printError(const CompilerErrorException &ce) {
    hasError = true;
    if (outputJSON) {
        fprintf(stderr, "%s{\"type\": \"error\", \"line\": %zu, \"character\": %zu, \"file\":", printedErrorOrWarning ? ",": "",
                ce.position().line, ce.position().character);
        printJSONStringToFile(ce.position().file, stderr);
        fprintf(stderr, ", \"message\":");
        printJSONStringToFile(ce.error(), stderr);
        fprintf(stderr, "}\n");
    }
    else {
        fprintf(stderr, "🚨 line %zu column %zu %s: %s\n", ce.position().line, ce.position().character,
                ce.position().file, ce.error());
    }
    printedErrorOrWarning = true;
}

void compilerWarning(SourcePosition p, const char *err, ...) {
    va_list list;
    va_start(list, err);
    
    char error[450];
    vsprintf(error, err, list);
    
    if (outputJSON) {
        fprintf(stderr, "%s{\"type\": \"warning\", \"line\": %zu, \"character\": %zu, \"file\":", printedErrorOrWarning ? ",": "",
                p.line, p.character);
        printJSONStringToFile(p.file, stderr);
        fprintf(stderr, ", \"message\":");
        printJSONStringToFile(error, stderr);
        fprintf(stderr, "}\n");
    }
    else {
        fprintf(stderr, "⚠️ line %zu col %zu %s: %s\n", p.line, p.character, p.file, error);
    }
    printedErrorOrWarning = true;
    
    va_end(list);
}

Class* getStandardClass(EmojicodeChar name, Package *_, SourcePosition errorPosition) {
    Type type = typeNothingness;
    _->fetchRawType(name, globalNamespace, false, errorPosition, &type);
    if (type.type() != TT_CLASS) {
        ecCharToCharStack(name, nameString)
        throw CompilerErrorException(errorPosition, "s package class %s is missing.", nameString);
    }
    return type.eclass;
}

Protocol* getStandardProtocol(EmojicodeChar name, Package *_, SourcePosition errorPosition) {
    Type type = typeNothingness;
    _->fetchRawType(name, globalNamespace, false, errorPosition, &type);
    if (type.type() != TT_PROTOCOL) {
        ecCharToCharStack(name, nameString)
        throw CompilerErrorException(errorPosition, "s package protocol %s is missing.", nameString);
    }
    return type.protocol;
}

void loadStandard(Package *_, SourcePosition errorPosition) {
    auto package = _->loadPackage("s", globalNamespace, errorPosition);
    
    CL_STRING = getStandardClass(0x1F521, _, errorPosition);
    CL_LIST = getStandardClass(0x1F368, _, errorPosition);
    CL_ERROR = getStandardClass(0x1F6A8, _, errorPosition);
    CL_DATA = getStandardClass(0x1F4C7, _, errorPosition);
    CL_DICTIONARY = getStandardClass(0x1F36F, _, errorPosition);
    CL_RANGE = getStandardClass(0x23E9, _, errorPosition);
    
    PR_ENUMERATOR = getStandardProtocol(0x1F361, _, errorPosition);
    PR_ENUMERATEABLE = getStandardProtocol(E_CLOCKWISE_RIGHTWARDS_AND_LEFTWARDS_OPEN_CIRCLE_ARROWS_WITH_CIRCLED_ONE_OVERLAY, _, errorPosition);
    
    package->setRequiresBinary(false);
}

int main(int argc, char * argv[]) {
    const char *reportPackage = nullptr;
    char *outPath = nullptr;
    
    signed char ch;
    while ((ch = getopt(argc, argv, "vrjR:o:")) != -1) {
        switch (ch) {
            case 'v':
                puts("Emojicode Compiler 1.0.0alpha1. Emojicode 0.2. Built with 💚 by Theo Weidmann.");
                return 0;
                break;
            case 'R':
                reportPackage = optarg;
                break;
            case 'r':
                reportPackage = "_";
                break;
            case 'o':
                outPath = optarg;
                break;
            case 'j':
                outputJSON = true;
                break;
            default:
                break;
        }
    }
    argc -= optind;
    argv += optind;
    
    if (outputJSON) {
        fprintf(stderr, "[");
    }
    
    if (argc == 0) {
        compilerWarning(SourcePosition(0, 0, ""), "No input files provided.");
        return 1;
    }
    
    if (outPath == nullptr) {
        outPath = strdup(argv[0]);
        outPath[strlen(outPath) - 1] = 'b';
    }
    
    foundStartingFlag = false;
    
    Package pkg = Package("_");
    pkg.setPackageVersion(PackageVersion(1, 0));
    
    auto errorPosition = SourcePosition(0, 0, argv[0]);
    
    FILE *out = fopen(outPath, "wb");
    
    try {
        loadStandard(&pkg, errorPosition);
        
        pkg.parse(argv[0]);
        
        if (!out || ferror(out)) {
            throw CompilerErrorException(errorPosition, "Couldn't write output file.");
            return 1;
        }
        
        if (!foundStartingFlag) {
            throw CompilerErrorException(errorPosition, "No 🏁 eclass method was found.");
        }
        
        analyzeClassesAndWrite(out);
    }
    catch (CompilerErrorException &ce) {
        printError(ce);
    }
    
    fclose(out);
    
    if (outputJSON) {
        fprintf(stderr, "]");
    }
    
    if (hasError) {
        unlink(outPath);
    }
    
    if (reportPackage) {
        if (auto package = Package::findPackage(reportPackage)) {
            report(package);
        }
        else {
            compilerWarning(errorPosition, "Report for package %s failed as it was not loaded.", reportPackage);
        }
    }
    
    return 0;
}
