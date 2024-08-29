v_major = 0
v_minor = 3
v_patch = 0
v_phase = \"\\\"alpha\\\"\"

# Set compiled app version
VERSION = $${v_major}.$${v_minor}.$${v_patch}

# Set version in source code
DEFINES += STRTB_SRC_VERSION_MAJOR=$${v_major} \
           STRTB_SRC_VERSION_MINOR=$${v_minor} \
           STRTB_SRC_VERSION_PATCH=$${v_patch} \
           STRTB_SRC_VERSION_PHASE=$${v_phase}
