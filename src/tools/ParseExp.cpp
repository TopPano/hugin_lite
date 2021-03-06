// -*- c-basic-offset: 4 -*-

/** @file ParseExp.cpp
 *
 *  @brief functions to parse expressions from strings
 *
 *  @author T. Modes
 *
 */

/*  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This software is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this software; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

// implementation is based on blog at
// http://agentzlerich.blogspot.de/2011/06/using-boost-spirit-21-to-evaluate.html
// modified to Hugins need
// added if statement

#include "ParseExp.h"

#include <limits>
#include <iterator>

#include <boost/spirit/version.hpp>
#if !defined(SPIRIT_VERSION) || SPIRIT_VERSION < 0x2010
#error "At least Spirit version 2.1 required"
#endif
#include <boost/math/constants/constants.hpp>
#include <boost/spirit/include/phoenix.hpp>
#include <boost/spirit/include/qi.hpp>

namespace Parser
{

// helper classes to implement operators

//power function
struct lazy_pow_
{
    template <typename X, typename Y>
    struct result { typedef X type; };

    template <typename X, typename Y>
    X operator()(X x, Y y) const
    {
        return std::pow(x, y);
    }
};

// modulus for double values
struct lazy_mod_
{
    template <typename X, typename Y>
    struct result { typedef X type; };

    template <typename X, typename Y>
    X operator()(X x, Y y) const
    {
        return std::fmod(x,y);
    }
};

// if statement
struct lazy_if_
{
    template <typename X, typename Y, typename Z>
    struct result { typedef Y type; };

    template <typename X, typename Y, typename Z>
    X operator()(X x, Y y, Z z) const
    {
        return x ? y : z;
    }
};

// wrapper for unary function
struct lazy_ufunc_
{
    template <typename F, typename A1>
    struct result { typedef A1 type; };

    template <typename F, typename A1>
    A1 operator()(F f, A1 a1) const
    {
        return f(a1);
    }
};

// convert rad into deg
double deg(const double d)
{
    return d*180.0/boost::math::constants::pi<double>();
};

// convert deg into rad
double rad(const double d)
{
    return d*boost::math::constants::pi<double>()/180;
};

// the main grammar class
struct grammar:boost::spirit::qi::grammar<std::string::const_iterator, double(), boost::spirit::ascii::space_type>
{

    // symbol table for constants like "pi", e.g. image number and value
    struct constant_ : boost::spirit::qi::symbols<char, double>
    {
        constant_(const ConstantMap constMap)
        {
            this->add("pi", boost::math::constants::pi<double>());
            if(constMap.size()>0)
            {
                for(ConstantMap::const_iterator it=constMap.begin(); it!=constMap.end(); it++)
                {
                    this->add(it->first, it->second); 
                };
            };
        };
    };

    // symbol table for unary functions like "abs"
    struct ufunc_  : boost::spirit::qi::symbols<char, double(*)(double) >
    {
        ufunc_()
        {
            this->add
                ("abs"   , (double (*)(double)) std::abs  )
                ("acos"  , (double (*)(double)) std::acos )
                ("asin"  , (double (*)(double)) std::asin )
                ("atan"  , (double (*)(double)) std::atan )
                ("ceil"  , (double (*)(double)) std::ceil )
                ("sin"   , (double (*)(double)) std::sin  )
                ("cos"   , (double (*)(double)) std::cos  )
                ("tan"   , (double (*)(double)) std::tan  )
                ("exp"   , (double (*)(double)) std::exp  )
                ("floor" , (double (*)(double)) std::floor)
                ("sqrt"  , (double (*)(double)) std::sqrt )
                ("deg"   , (double (*)(double)) deg  )
                ("rad"   , (double (*)(double)) rad  )
            ;
        }
    } ufunc;

    boost::spirit::qi::rule<std::string::const_iterator, double(), boost::spirit::ascii::space_type> expression, term, factor, primary, compExpression, compTerm, numExpression;

    grammar(const ConstantMap constMap) : grammar::base_type(expression)
    {
        using boost::spirit::qi::real_parser;
        using boost::spirit::qi::real_policies;
        real_parser<double,real_policies<double> > real;

        using boost::spirit::qi::_1;
        using boost::spirit::qi::_2;
        using boost::spirit::qi::_3;
        using boost::spirit::qi::no_case;
        using boost::spirit::qi::_val;
        struct constant_ constant(constMap);

        boost::phoenix::function<lazy_pow_>   lazy_pow;
        boost::phoenix::function<lazy_mod_>   lazy_mod;
        boost::phoenix::function<lazy_if_>    lazy_if;
        boost::phoenix::function<lazy_ufunc_> lazy_ufunc;

        expression = 
            (compExpression >> '\?' >> compExpression >> ':' >> compExpression) [_val = lazy_if(_1, _2, _3)]
            | compExpression [_val=_1]
            ;
        
        compExpression=
            compTerm  [_val=_1]
            >> * ( ("&&" >> compTerm [_val = _val && _1] )
                  |("||" >> compTerm [_val = _val || _1] )
                 )
            ;

        compTerm =
            numExpression                [_val = _1        ]
            >>*( ( '<'  >> numExpression [_val = _val <  _1])
                |( '>'  >> numExpression [_val = _val >  _1])
                |( "<=" >> numExpression [_val = _val <= _1])
                |( ">=" >> numExpression [_val = _val >= _1])
                |( "==" >> numExpression [_val = _val == _1])
                |( "!=" >> numExpression [_val = _val != _1])
               )
            ;

        numExpression =
            term                   [_val =  _1]
            >> *(  ('+' >> term    [_val += _1])
                |  ('-' >> term    [_val -= _1])
                )
            ;

        term =
            factor                 [_val =  _1]
            >> *(  ('*' >> factor  [_val *= _1])
                |  ('/' >> factor  [_val /= _1])
                |  ('%' >> factor  [_val = lazy_mod(_val, _1)])
                )
            ;

        factor =
            primary                [_val =  _1]
            >> *(  ('^' >> factor [_val = lazy_pow(_val, _1)]) )
            ;

        primary =
            real                   [_val =  _1]
            |  '(' >> expression   [_val =  _1] >> ')'
            |  ('-' >> primary     [_val = -_1])
            |  ('+' >> primary     [_val =  _1])
            |  no_case[constant]   [_val =  _1]
            |  (no_case[ufunc] >> '(' >> expression >> ')') [_val = lazy_ufunc(_1, _2) ]
            ;

    };
};

//template <typename ParserType, typename Iterator>
bool parse(std::string::const_iterator &iter,
           std::string::const_iterator end,
           const grammar &g,
           double& result)
{
    if(!boost::spirit::qi::phrase_parse(iter, end, g, boost::spirit::ascii::space, result))
    {
        return false;
    };
    // we check if the full string could parsed
    return iter==end;
}

// the function which exposes the interface to external
// version without pre-defined constants 
bool ParseExpression(const std::string& expression, double& result)
{
    ConstantMap constants;
    return ParseExpression(expression, result, constants);
};
    
// version with pre-defined constants
bool ParseExpression(const std::string& expression, double& result, const ConstantMap& constants)
{
    grammar g(constants);
    std::string::const_iterator it=expression.begin();
    return parse(it, expression.end(), g, result);
};

} // namespace
