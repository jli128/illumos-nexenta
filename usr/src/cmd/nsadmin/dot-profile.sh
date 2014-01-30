#
# Use less(1) as the default pager for the man(1) command.
#
export PATH=/usr/bin:/usr/sbin:/sbin
export PAGER="/usr/bin/less -ins"

#
# Define default prompt to <username>@<hostname>:<path><"($|#) ">
# and print '#' for user "root" and '$' for normal users.
#
PS1='${LOGNAME}@$(/usr/bin/hostname):$(
    [[ "${LOGNAME}" == "root" ]] && printf "%s" "${PWD/${HOME}/~}# " ||
    printf "%s" "${PWD/${HOME}/~}\$ ")'
