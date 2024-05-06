
#######################################
# Show and execute command
#
# Arguments:
#   $@ - command with parameters
# Returns:
#   None
#######################################
run() {
    WHITE_LIGHT="\033[1;37m"
    NOCOLOR="\033[0m"
    echo -e "$WHITE_LIGHT $@ $NOCOLOR" >&2
    $@
}



#######################################
# Calculate relative path
#
# Arguments:
#   $1 - path
#   $2 - start directory (optional)
# Returns:
#   None
#######################################
relpath() {
    python3 -c "import os.path; print(os.path.relpath('$1','${2:-$PWD}'))" ;
}

