#<cldoc:Return code>

Return code definition


Return code definition -

>0x{production} {itemdict} {actiondict} {resourcedict} {statusdict}

- production: "20"
- itemdict: {itemdict}
- actiondict: {actiondict}
- resourcedict: {resourcedict}
- statusdict: {statusdict}

Example return code -
>0x2001010101 means "Set config was successful."

Fields -

|{itemdict}|{actiondict}|{resourcedict}|{statusdict}|
|----------|:-----------|:-------------|:-----------|
|"API":"01"|"set":"01"|"config":"01"|"successful":"001"|
|          |"get":"02"|"property":"02"|Linux System Errors:001 - 07C|
|          |"pin":"03"|"file":"03"||
|          |"unpin":"04"|"app":"04"||
|          |"query":"05"|"stat":"05"||
