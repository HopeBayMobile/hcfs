#<cldoc:Return value>

Return value definition


Return value definition -

>```json
{
    result: boolean,
    code: int,
    data: dictionary
}
```

Fields -

- result: To determine if this operation is successful or not.
- code: More details about this operation. (Code is defined in section of each API.)
- data: Additional key/value pairs returned by this operation. (Data is defined in section of each API.)

Example -

>```json
{
    result: True,
    code: 0,
    data: {
        key1: val1,
        key2: val2,
    }
}
```


