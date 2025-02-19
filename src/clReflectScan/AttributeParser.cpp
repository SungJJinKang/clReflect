
//
// ===============================================================================
// clReflect
// -------------------------------------------------------------------------------
// Copyright (c) 2011-2012 Don Williamson & clReflect Authors (see AUTHORS file)
// Released under MIT License (see LICENSE file)
// ===============================================================================
//

#include "AttributeParser.h"

#include <clReflectCore/Database.h>
#include <clReflectCore/FileUtils.h>
#include <clReflectCore/Logging.h>

#include <ctype.h>
#include <string.h>

namespace
{
    // Error reporting feedback
    const char* g_Filename = 0;
    int g_Line = 0;

    enum TokenType
    {
        TOKEN_NONE,
        TOKEN_EQUALS,
        TOKEN_COMMA,
        TOKEN_INT,
        TOKEN_HEX_INT,
        TOKEN_FLOAT,
        TOKEN_SYMBOL,
        TOKEN_STRING,
    };

    struct Token
    {
        Token()
            : type(TOKEN_NONE)
            , ptr(0)
            , length(0)
        {
        }

        Token(TokenType t, const char* p, int l)
            : type(t)
            , ptr(p)
            , length(l)
        {
        }

        const char* GetText() const
        {
            // Copy locally to a static string and return that after null terminating
            thread_local static char text[1024];
            int l = length >= sizeof(text) ? sizeof(text) - 1 : length;
            strncpy(text, ptr, l);
            text[l] = 0;
            return text;
        }

        TokenType type;
        const char* ptr;
        int length;
    };

    const char* ParseString(const char* text, std::vector<Token>& tokens)
    {
        // Start one character after the quote and loop until the end
        const char* start = ++text;
        while (*text && *text != '\"')
        {
            text++;
        }

        // If the string terminated correctly, add it
        if (*text == '\"')
        {
            tokens.push_back(Token(TOKEN_STRING, start, text - start));
            return text + 1;
        }

        LOG(warnings, INFO, "%s(%d) : warning - String not terminated correctly\n", g_Filename, g_Line);
        return 0;
    }

    const char* ParseSymbol(const char* text, std::vector<Token>& tokens)
    {
        // Match the pattern [A-Za-z0-9_:]*
        const char* start = text;
        while (*text && (isalnum(*text) || *text == '_' || *text == ':'))
        {
            text++;
        }

        // Add to the list of tokens
        tokens.push_back(Token(TOKEN_SYMBOL, start, text - start));
        return text;
    }

    const char* ParseNumber(const char* text, std::vector<Token>& tokens)
    {
        // Match all digits, taking into account this might be a hex or floating pointer number
        TokenType type = TOKEN_INT;
        const char* start = text;
        while (*text && (isxdigit(*text) || *text == '.' || *text == 'x' || *text == 'X'))
        {
            switch (*text)
            {
            case '.':
            case 'x':
            case 'X':
                // Prevent multiple occurrences
                if (type != TOKEN_INT)
                {
                    LOG(warnings, INFO, "%s(%d) : warning - invalid integer representation\n", g_Filename, g_Line);
                    return 0;
                }

                // Decide what to transition to
                if (*text == '.')
                    type = TOKEN_FLOAT;
                else
                    type = TOKEN_HEX_INT;
            }

            text++;
        }

        // Add the number token based on float/int property
        tokens.push_back(Token(type, start, text - start));
        return text;
    }

    std::vector<Token> Lexer(const char* text)
    {
        // Tokenise the input character stream
        std::vector<Token> tokens;
        while (char c = *text)
        {
            // Trying to use the compiler to generate an efficient lookup table for all the single
            // characters that kick off pattern matching for each token.
            switch (c)
            {
                // Process single character tokens
            case ('='):
                tokens.push_back(Token(TOKEN_EQUALS, text, 1));
                text++;
                break;
            case (','):
                tokens.push_back(Token(TOKEN_COMMA, text, 1));
                text++;
                break;

                // Process strings
            case ('\"'):
                text = ParseString(text, tokens);
                break;

                // Skip whitespace
            case (' '):
            case ('\t'):
                text++;
                break;

                // Process symbols that start with underscore
            case ('_'):
                text = ParseSymbol(text, tokens);
                break;

            default:
                // Handle the text range [A-Za-z]
                if (isalpha(c))
                {
                    text = ParseSymbol(text, tokens);
                }

                // Handle the number range [0-9]
                else if (isdigit(c))
                {
                    text = ParseNumber(text, tokens);
                }

                else
                {
                    LOG(warnings, INFO, "%s(%d) : warning - Invalid character in attribute\n", g_Filename, g_Line);
                    text = 0;
                }
            }

            // An error has been signalled above so abort lexing and clear the tokens so no parsing occurs
            if (text == 0)
            {
                tokens.clear();
                break;
            }
        }

        return tokens;
    }

    const Token* ExpectNext(const std::vector<Token>& tokens, size_t& pos, TokenType type)
    {
        if (tokens[pos].type == type)
        {
            return &tokens[pos++];
        }
        return 0;
    }

    const Token* CheckNext(const std::vector<Token>& tokens, size_t& pos, TokenType type)
    {
        // Keep within token stream limits
        if (pos >= tokens.size())
        {
            return 0;
        }

        // Increment and return if there's a match
        if (tokens[pos].type == type)
        {
            return &tokens[pos++];
        }

        return 0;
    }

    //
    // Overloads for adding attribute pointers to a vector that need to be released at a later point
    //
    void AddFlagAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name)
    {
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::FlagAttribute(name, cldb::Name()));
    }
    void AddIntAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name,
                         const Token& val)
    {
        int value = atoi(val.GetText());
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::IntAttribute(name, cldb::Name(), value));
    }
    void AddHexIntAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name,
                            const Token& val)
    {
        int value = 0;
        const char* text = val.GetText();
        if (text[0] == '0' && (text[1] == 'x' || text[2] == 'X'))
            value = hextoi(text + 2);
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::IntAttribute(name, cldb::Name(), value));
    }
    void AddFloatAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name,
                           const Token& val)
    {
        float value = 0;
        sscanf(val.GetText(), "%f", &value);
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::FloatAttribute(name, cldb::Name(), value));
    }
    void AddPrimitiveAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name,
                               const Token& val)
    {
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::PrimitiveAttribute(name, cldb::Name(), db.GetName(val.GetText())));
    }
    void AddTextAttribute(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const Token* attribute_name,
                          const Token& val)
    {
        cldb::Name name = db.GetName(attribute_name->GetText());
        attributes.push_back(new cldb::TextAttribute(name, cldb::Name(), val.GetText()));
    }

    bool AttributeDef(cldb::Database& db, std::vector<cldb::Attribute*>& attributes, const std::vector<Token>& tokens,
                      size_t& pos)
    {
        // Expect a symbol to start the attribute
        const Token* attribute_name = ExpectNext(tokens, pos, TOKEN_SYMBOL);
        if (attribute_name == 0)
        {
            LOG(warnings, INFO, "%s(%d) : warning - Symbol expected in attribute\n", g_Filename, g_Line);
            return false;
        }

        // Check for an assignment and consume it
        if (CheckNext(tokens, pos, TOKEN_EQUALS))
        {
            if (pos >= tokens.size())
            {
                LOG(warnings, INFO, "%s(%d) : warning - Value expected at the end of the attribute\n", g_Filename, g_Line);
                return false;
            }

            // Create the attribute based on what the next token is
            const Token& val = tokens[pos++];
            switch (val.type)
            {
            case (TOKEN_INT):
                AddIntAttribute(db, attributes, attribute_name, val);
                break;
            case (TOKEN_HEX_INT):
                AddHexIntAttribute(db, attributes, attribute_name, val);
                break;
            case (TOKEN_FLOAT):
                AddFloatAttribute(db, attributes, attribute_name, val);
                break;
            case (TOKEN_SYMBOL):
                AddPrimitiveAttribute(db, attributes, attribute_name, val);
                break;
            case (TOKEN_STRING):
                AddTextAttribute(db, attributes, attribute_name, val);
                break;
            default:
                LOG(warnings, INFO, "%s(%d) : warning - Value expected for attribute assignment\n", g_Filename, g_Line);
                return false;
            }
        }

        else
        {
            AddFlagAttribute(db, attributes, attribute_name);
        }

        return true;
    }

    std::vector<cldb::Attribute*> Parser(cldb::Database& db, const std::vector<Token>& tokens)
    {
        // Don't parse if there are no tokens (this could be a lexer error or an explicit line code)
        std::vector<cldb::Attribute*> attributes;
        if (tokens.empty())
        {
            return attributes;
        }

        // Parse the first attribute
        size_t pos = 0;
        if (!AttributeDef(db, attributes, tokens, pos))
        {
            return attributes;
        }

        // Loop parsing any remaining attributes
        while (pos < tokens.size() && tokens[pos].type == TOKEN_COMMA)
        {
            pos++;
            if (!AttributeDef(db, attributes, tokens, pos))
            {
                return attributes;
            }
        }

        return attributes;
    }
}

std::vector<cldb::Attribute*> ParseAttributes(cldb::Database& db, const char* text, const char* filename, int line)
{
    g_Filename = filename;
    g_Line = line;

    // Make things a little simpler by lexing all tokens at once before parsing
    std::vector<Token> tokens = Lexer(text);
    return Parser(db, tokens);
}
