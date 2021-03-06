/***************************************************************************
                               qgsexpression.h
                             -------------------
    begin                : August 2011
    copyright            : (C) 2011 Martin Dobias
    email                : wonder.sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSEXPRESSION_H
#define QGSEXPRESSION_H

#include "qgis_core.h"
#include <QMetaType>
#include <QStringList>
#include <QVariant>
#include <QList>
#include <QDomDocument>
#include <QCoreApplication>
#include <QSet>
#include <functional>

#include "qgis.h"
#include "qgsunittypes.h"
#include "qgsinterval.h"

class QgsFeature;
class QgsGeometry;
class QgsOgcUtils;
class QgsVectorLayer;
class QgsVectorDataProvider;
class QgsField;
class QgsFields;
class QgsDistanceArea;
class QDomElement;
class QgsExpressionContext;
class QgsExpressionPrivate;
class QgsExpressionNode;
class QgsExpressionFunction;

/**
 * \ingroup core
Class for parsing and evaluation of expressions (formerly called "search strings").
The expressions try to follow both syntax and semantics of SQL expressions.

Usage:
\code{.cpp}
  QgsExpression exp("gid*2 > 10 and type not in ('D','F')");
  if (exp.hasParserError())
  {
    // show error message with parserErrorString() and exit
  }
  QVariant result = exp.evaluate(feature, fields);
  if (exp.hasEvalError())
  {
    // show error message with evalErrorString()
  }
  else
  {
    // examine the result
  }
\endcode

Three Value Logic
=================

Similarly to SQL, this class supports three-value logic: true/false/unknown.
Unknown value may be a result of operations with missing data (NULL). Please note
that NULL is different value than zero or an empty string. For example
3 > NULL returns unknown.

There is no special (three-value) 'boolean' type: true/false is represented as
1/0 integer, unknown value is represented the same way as NULL values: NULL QVariant.

Performance
===========

For better performance with many evaluations you may first call prepare(fields) function
to find out indices of columns and then repeatedly call evaluate(feature).

Type conversion
===============

Operators and functions that expect arguments to be of a particular
type automatically convert the arguments to that type, e.g. sin('2.1') will convert
the argument to a double, length(123) will first convert the number to a string.
Explicit conversion can be achieved with to_int, to_real, to_string functions.
If implicit or explicit conversion is invalid, the evaluation returns an error.
Comparison operators do numeric comparison in case both operators are numeric (int/double)
or they can be converted to numeric types.

Implicit sharing
================

This class is implicitly shared, copying has a very low overhead.
It is normally preferable to call `QgsExpression( otherExpression )` instead of
`QgsExpression( otherExpression.expression() )`. A deep copy will only be made
when prepare() is called. For usage this means mainly, that you should
normally keep an unprepared master copy of a QgsExpression and whenever using it
with a particular QgsFeatureIterator copy it just before and prepare it using the
same context as the iterator.

Implicit sharing was added in 2.14

*/

class CORE_EXPORT QgsExpression
{
    Q_DECLARE_TR_FUNCTIONS( QgsExpression )
  public:

    /**
     * Creates a new expression based on the provided string.
     * The string will immediately be parsed. For optimization
     * prepare() should always be called before every
     * loop in which this expression is used.
     */
    QgsExpression( const QString &expr );

    /**
     * Create a copy of this expression. This is preferred
     * over recreating an expression from a string since
     * it does not need to be re-parsed.
     */
    QgsExpression( const QgsExpression &other );

    /**
     * Create a copy of this expression. This is preferred
     * over recreating an expression from a string since
     * it does not need to be re-parsed.
     */
    QgsExpression &operator=( const QgsExpression &other );

    /**
     * Automatically convert this expression to a string where requested.
     *
     * \since QGIS 3.0
     */
    operator QString() const SIP_SKIP;

    /**
     * Create an empty expression.
     *
     * \since QGIS 3.0
     */
    QgsExpression();

    ~QgsExpression();

    /**
     * Compares two expressions. The operator returns true
     * if the expression string is equal.
     *
     * \since QGIS 3.0
     */
    bool operator==( const QgsExpression &other ) const;

    /**
     * Checks if this expression is valid.
     * A valid expression could be parsed but does not necessarily evaluate properly.
     *
     * \since QGIS 3.0
     */
    bool isValid() const;

    //! Returns true if an error occurred when parsing the input expression
    bool hasParserError() const;
    //! Returns parser error
    QString parserErrorString() const;

    //! Returns root node of the expression. Root node is null is parsing has failed
    const QgsExpressionNode *rootNode() const;

    /**
     * Get the expression ready for evaluation - find out column indexes.
     * \param context context for preparing expression
     * \since QGIS 2.12
     */
    bool prepare( const QgsExpressionContext *context );

    /**
     * Get list of columns referenced by the expression.
     *
     * \note If the returned list contains the QgsFeatureRequest::AllAttributes constant then
     * all attributes from the layer are required for evaluation of the expression.
     * QgsFeatureRequest::setSubsetOfAttributes automatically handles this case.
     *
     * \see referencedAttributeIndexes()
     */
    QSet<QString> referencedColumns() const;

    /**
     * Return a list of all variables which are used in this expression.
     * If the list contains a NULL QString, there is a variable name used
     * which is determined at runtime.
     *
     * \since QGIS 3.0
     */
    QSet<QString> referencedVariables() const;

    /**
     * Return a list of field name indexes obtained from the provided fields.
     *
     * \since QGIS 3.0
     */
    QSet<int> referencedAttributeIndexes( const QgsFields &fields ) const;

    //! Returns true if the expression uses feature geometry for some computation
    bool needsGeometry() const;

    // evaluation

    /**
     * Evaluate the feature and return the result.
     * \note this method does not expect that prepare() has been called on this instance
     * \since QGIS 2.12
     */
    QVariant evaluate();

    /**
     * Evaluate the expression against the specified context and return the result.
     * \param context context for evaluating expression
     * \note prepare() should be called before calling this method.
     * \since QGIS 2.12
     */
    QVariant evaluate( const QgsExpressionContext *context );

    //! Returns true if an error occurred when evaluating last input
    bool hasEvalError() const;
    //! Returns evaluation error
    QString evalErrorString() const;
    //! Set evaluation error (used internally by evaluation functions)
    void setEvalErrorString( const QString &str );

    /**
     * Checks whether an expression consists only of a single field reference
     * \since QGIS 2.9
     */
    bool isField() const;

    /**
     * Tests whether a string is a valid expression.
     * \param text string to test
     * \param context optional expression context
     * \param errorMessage will be filled with any error message from the validation
     * \returns true if string is a valid expression
     * \since QGIS 2.12
     */
    static bool checkExpression( const QString &text, const QgsExpressionContext *context, QString &errorMessage SIP_OUT );

    /**
     * Set the expression string, will reset the whole internal structure.
     *
     * \since QGIS 3.0
     */
    void setExpression( const QString &expression );

    /**
     * Return the original, unmodified expression string.
     * If there was none supplied because it was constructed by sole
     * API calls, dump() will be used to create one instead.
     */
    QString expression() const;

    /**
     * Return an expression string, constructed from the internal
     * abstract syntax tree. This does not contain any nice whitespace
     * formatting or comments. In general it is preferable to use
     * expression() instead.
     */
    QString dump() const;

    /**
     * Return calculator used for distance and area calculations
     * (used by $length, $area and $perimeter functions only)
     * \see setGeomCalculator()
     * \see distanceUnits()
     * \see areaUnits()
     */
    QgsDistanceArea *geomCalculator();

    /**
     * Sets the geometry calculator used for distance and area calculations in expressions.
     * (used by $length, $area and $perimeter functions only). By default, no geometry
     * calculator is set and all distance and area calculations are performed using simple
     * Cartesian methods (ie no ellipsoidal calculations).
     * \param calc geometry calculator. Ownership is not transferred. Set to a nullptr to force
     * Cartesian calculations.
     * \see geomCalculator()
     */
    void setGeomCalculator( const QgsDistanceArea *calc );

    /**
     * Returns the desired distance units for calculations involving geomCalculator(), e.g., "$length" and "$perimeter".
     * \note distances are only converted when a geomCalculator() has been set
     * \since QGIS 2.14
     * \see setDistanceUnits()
     * \see areaUnits()
     */
    QgsUnitTypes::DistanceUnit distanceUnits() const;

    /**
     * Sets the desired distance units for calculations involving geomCalculator(), e.g., "$length" and "$perimeter".
     * \note distances are only converted when a geomCalculator() has been set
     * \since QGIS 2.14
     * \see distanceUnits()
     * \see setAreaUnits()
     */
    void setDistanceUnits( QgsUnitTypes::DistanceUnit unit );

    /**
     * Returns the desired areal units for calculations involving geomCalculator(), e.g., "$area".
     * \note areas are only converted when a geomCalculator() has been set
     * \since QGIS 2.14
     * \see setAreaUnits()
     * \see distanceUnits()
     */
    QgsUnitTypes::AreaUnit areaUnits() const;

    /**
     * Sets the desired areal units for calculations involving geomCalculator(), e.g., "$area".
     * \note areas are only converted when a geomCalculator() has been set
     * \since QGIS 2.14
     * \see areaUnits()
     * \see setDistanceUnits()
     */
    void setAreaUnits( QgsUnitTypes::AreaUnit unit );

    /**
     * This function replaces each expression between [% and %]
     * in the string with the result of its evaluation with the specified context
     *
     * Additional substitutions can be passed through the substitutionMap parameter
     * \param action The source string in which placeholders should be replaced.
     * \param context Expression context
     * \param distanceArea Optional QgsDistanceArea. If specified, the QgsDistanceArea is used for distance
     * and area conversion
     * \since QGIS 2.12
     */
    static QString replaceExpressionText( const QString &action, const QgsExpressionContext *context,
                                          const QgsDistanceArea *distanceArea = nullptr );

    /**
     * Attempts to evaluate a text string as an expression to a resultant double
     * value.
     * \param text text to evaluate as expression
     * \param fallbackValue value to return if text can not be evaluated as a double
     * \returns evaluated double value, or fallback value
     * \since QGIS 2.7
     * \note this method is inefficient for bulk evaluation of expressions, it is intended
     * for one-off evaluations only.
     */
    static double evaluateToDouble( const QString &text, const double fallbackValue );

    enum SpatialOperator
    {
      soBbox,
      soIntersects,
      soContains,
      soCrosses,
      soEquals,
      soDisjoint,
      soOverlaps,
      soTouches,
      soWithin,
    };

    //! \note not available in Python bindings
    static QList<QgsExpressionFunction *> sFunctions SIP_SKIP;
    static const QList<QgsExpressionFunction *> &Functions();

    //! \note not available in Python bindings
    static QStringList sBuiltinFunctions SIP_SKIP;
    static const QStringList &BuiltinFunctions();

    /**
     * Registers a function to the expression engine. This is required to allow expressions to utilize the function.
     * \param function function to register
     * \param transferOwnership set to true to transfer ownership of function to expression engine
     * \returns true on successful registration
     * \see unregisterFunction
     */
    static bool registerFunction( QgsExpressionFunction *function, bool transferOwnership = false );

    /**
     * Unregisters a function from the expression engine. The function will no longer be usable in expressions.
     * \param name function name
     * \see registerFunction
     */
    static bool unregisterFunction( const QString &name );

    /**
     * List of functions owned by the expression engine
     * \note not available in Python bindings
     */
    static QList<QgsExpressionFunction *> sOwnedFunctions SIP_SKIP;

    /**
     * Deletes all registered functions whose ownership have been transferred to the expression engine.
     * \since QGIS 2.12
     */
    static void cleanRegisteredFunctions();

    //! tells whether the identifier is a name of existing function
    static bool isFunctionName( const QString &name );

    //! return index of the function in Functions array
    static int functionIndex( const QString &name );

    /**
     * Returns the number of functions defined in the parser
     * \returns The number of function defined in the parser.
     */
    static int functionCount();

    /**
     * Returns a quoted column reference (in double quotes)
     * \see quotedString()
     * \see quotedValue()
     */
    static QString quotedColumnRef( QString name );

    /**
     * Returns a quoted version of a string (in single quotes)
     * \see quotedValue()
     * \see quotedColumnRef()
     */
    static QString quotedString( QString text );

    /**
     * Returns a string representation of a literal value, including appropriate
     * quotations where required.
     * \param value value to convert to a string representation
     * \since QGIS 2.14
     * \see quotedString()
     * \see quotedColumnRef()
     */
    static QString quotedValue( const QVariant &value );

    /**
     * Returns a string representation of a literal value, including appropriate
     * quotations where required.
     * \param value value to convert to a string representation
     * \param type value type
     * \since QGIS 2.14
     * \see quotedString()
     * \see quotedColumnRef()
     */
    static QString quotedValue( const QVariant &value, QVariant::Type type );

    //////

    /**
     * Returns the help text for a specified function.
     * \param name function name
     * \see variableHelpText()
     * \see formatVariableHelp()
     */
    static QString helpText( QString name );

    /**
     * Returns the help text for a specified variable.
     * \param variableName name of variable
     * \see helpText()
     * \since QGIS 2.12
     */
    static QString variableHelpText( const QString &variableName );

    /**
     * Returns formatted help text for a variable.
     * \param description translated description of variable
     * \param showValue set to true to include current value of variable in help text
     * \param value current value of variable to show in help text
     * \see helpText()
     * \see variableHelpText()
     * \since QGIS 3.0
     */
    static QString formatVariableHelp( const QString &description, bool showValue = true, const QVariant &value = QVariant() );

    /**
     * Returns the translated name for a function group.
     * \param group untranslated group name
     */
    static QString group( const QString &group );

    /**
     * Formats an expression result for friendly display to the user. Truncates the result to a sensible
     * length, and presents text representations of non numeric/text types (e.g., geometries and features).
     * \param value expression result to format
     * \returns formatted string, may contain HTML formatting characters
     * \since QGIS 2.14
     */
    static QString formatPreviewString( const QVariant &value );

    /**
     * Create an expression allowing to evaluate if a field is equal to a
     *  value. The value may be null.
     * \param fieldName the name of the field
     * \param value the value of the field
     * \returns the expression to evaluate field equality
     * \since QGIS 3.0
     */
    static QString createFieldEqualityExpression( const QString &fieldName, const QVariant &value );

  private:
    void initGeomCalculator();

    struct HelpArg SIP_SKIP
    {
      HelpArg( const QString &arg, const QString &desc, bool descOnly = false, bool syntaxOnly = false,
               bool optional = false, const QString &defaultVal = QString() )
        : mArg( arg )
        , mDescription( desc )
        , mDescOnly( descOnly )
        , mSyntaxOnly( syntaxOnly )
        , mOptional( optional )
        , mDefaultVal( defaultVal )
      {}

      QString mArg;
      QString mDescription;
      bool mDescOnly;
      bool mSyntaxOnly;
      bool mOptional;
      QString mDefaultVal;
    };

    struct HelpExample SIP_SKIP
    {
      HelpExample( const QString &expression, const QString &returns, const QString &note = QString() )
        : mExpression( expression )
        , mReturns( returns )
        , mNote( note )
      {}

      QString mExpression;
      QString mReturns;
      QString mNote;
    };

    struct HelpVariant SIP_SKIP
    {
      HelpVariant( const QString &name, const QString &description,
                   const QList<QgsExpression::HelpArg> &arguments = QList<QgsExpression::HelpArg>(),
                   bool variableLenArguments = false,
                   const QList<QgsExpression::HelpExample> &examples = QList<QgsExpression::HelpExample>(),
                   const QString &notes = QString() )
        : mName( name )
        , mDescription( description )
        , mArguments( arguments )
        , mVariableLenArguments( variableLenArguments )
        , mExamples( examples )
        , mNotes( notes )
      {}

      QString mName;
      QString mDescription;
      QList<QgsExpression::HelpArg> mArguments;
      bool mVariableLenArguments;
      QList<QgsExpression::HelpExample> mExamples;
      QString mNotes;
    };

    struct Help SIP_SKIP
    {
      //! Constructor for expression help
      Help() = default;

      Help( const QString &name, const QString &type, const QString &description, const QList<QgsExpression::HelpVariant> &variants )
        : mName( name )
        , mType( type )
        , mDescription( description )
        , mVariants( variants )
      {}

      QString mName;
      QString mType;
      QString mDescription;
      QList<QgsExpression::HelpVariant> mVariants;
    };

    /**
     * Helper for implicit sharing. When called will create
     * a new deep copy of this expression.
     *
     * \note not available in Python bindings
     */
    void detach() SIP_SKIP;

    QgsExpressionPrivate *d = nullptr;

    static QHash<QString, Help> sFunctionHelpTexts;
    static QHash<QString, QString> sVariableHelpTexts;
    static QHash<QString, QString> sGroups;

    //! \note not available in Python bindings
    static void initFunctionHelp() SIP_SKIP;
    //! \note not available in Python bindings
    static void initVariableHelp() SIP_SKIP;

    friend class QgsOgcUtils;
};

Q_DECLARE_METATYPE( QgsExpression )

#endif // QGSEXPRESSION_H
