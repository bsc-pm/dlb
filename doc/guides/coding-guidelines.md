# Coding guidelines

## Names
1. Use *DLB_PascalCase* for C interface function names.
1. Use *snake_case* for function names.
1. Use *snake_case* for local variables.
1. Use *UPPER_CASE* for enum values.
1. Use whole words in names when possible.

## Style
1. Use 4 spaces per indentation.
1. Braces should start on the same line as the statement.
1. Always surround loop and conditional bodies with curly braces.
    Statements on the same line are allowed to omit braces.
1. Type names should (in general) be defined inside the scope that uses them.
1. Error condition should be reported through return enum mechanism.

## Functions
1. Use static functions when possible.
1. Do not export a symbol name if it does not belong to the interface.

## Documentation
1. Use doxygen to document all functions that belong to the interface.
    * A brief summary (one sentence) declaring the function main purpose
        (mandatory).
    * A list of parameters (with its type [in], [out] or [in,out]) including a
        short description (mandatory, if any).
    * The returning value of the function (mandatory, if non-void).
    * A more detailed function description (when needed).
    * A doxygen example:

        ```c
        /*! \brief Brief function description
         *! \param[type] param1 Param 1 description
         *! \param[type] param2 Param 2 description
         *! \return Return value description
         */
        int DLB_Function(type1_t param1, type2_t, param2) {
            ...
        }
        ```

## License
1. All files (sources, headers, scripts, ...) MUST have the proper license
header

