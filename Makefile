### Type "make help" for details of usage.

VERSION=1.8

### INITIALIZE LOGGER

.SECONDARY_EXPANSION:
MAKE_DEBUG=
ifdef MAKE_DEBUG
log=$(warning $(1) $($(1)))
OLD_SHELL := $(SHELL)
SHELL = $(warning Building $@)$(OLD_SHELL)
endif


### BEGIN USER CONFIGURATION

# if you have source (.c/.cpp) files in any directory
# other than this dir, list the dirs here
# (after ".", separated by spaces)
# (note that munit [C test framework] is automatically handled if you are using C and it)

SOURCE_DIRS = . sqlite3

LDFLAGS =

# These are included for all three executables
# (note that Boost.test libraries are automatically
# handled -- no need to list here)

LDLIBS =-ldl -lpthread

# two special main programs. Release and debug 
# use MAIN_SRC, but unit tests use TEST_SRC
# These are expected to be the files that contain main()
# for the release/debug and test versions, respectively.
# (for MAIN_SRC, do not include any other C/CPP files than the one containing main())

###
### If these are left unset, they will be inferred
### from the existence of main.c/main.cpp/tests.c/tests.cpp
###

MAIN_SRC    :=

# if any test-specific source files, add them here

TEST_SRC		:=

# list any source files (directories if not in .) that
# are NOT part of test or release

EXCLUDE_SRC = sqlite3/shell.c

### END USER CONFIGURATION

### TARGET EXECUTABLE NAMES

RELEASE_EXE  =./main
TEST_EXE     =./tests
DEBUG_EXE    =$(RELEASE_EXE)-debug

### INFER MAIN() SOURCE FILE NAMES (and their language)

ifneq (($filter .,$(SOURCE_DIRS)),.)
	SOURCE_DIRS := . $(SOURCE_DIRS)
endif

ifndef MAIN_SRC
	MAIN_SRC := $(wildcard main.c main.cpp)
endif
ifndef MAIN_SRC
$(error Cannot determine main() file. Please configure MAIN_SRC)
endif
ifndef TEST_SRC
	TEST_SRC := $(wildcard tests.c tests.cpp)
endif
ifndef TEST_SRC
$(error Cannot determine test files (specifically test driver main() file). Please configure TEST_SRC)
endif

$(call log,MAIN_SRC)
$(call log,TEST_SRC)

###

### INFER TEST FRAMEWORK

# This (determining framework and commands) can 
# probably be left alone unless you are special
IS_C_MAIN    = $(MAIN_SRC:%.cpp=)
$(call log,IS_C_MAIN)

TEST_FRAMEWORK:=boost
ifneq ($(IS_C_MAIN),)
	TEST_FRAMEWORK:=munit
endif


ifeq ($(TEST_FRAMEWORK),munit)
	TEST_SRC    += munit/munit.c
	TEST_LDLIBS :=
	TEST_CMD    :=$(TEST_EXE) --color auto
	TEST_SELECTOR :=
	TEST_SUITELABEL :=@
	TEST_LIST :=--list
endif

ifeq ($(TEST_FRAMEWORK),boost)
	TEST_SRC    +=
	TEST_LDLIBS :=-Lboost_lib -rpath boost_lib -lboost_unit_test_framework
	TEST_CMD    :=$(TEST_EXE) --color-output --log_level=all
	TEST_SELECTOR :=--run_test=
	TEST_SUITELABEL :=@
	TEST_LIST :=--list_content
endif

$(call log,TEST_FRAMEWORK)
$(call log,TEST_CMD)

### END TEST FRAMEWORK



### PRE-TARGET COMPUTATIONS

# Various ways to list tests (Who remembers the correct one all the time?) "list<mumble>tests?"
LIST_TEST_VARIANTS=$(subst @@,,$(foreach S,test tests,\
	$(foreach SEP,- @@ _,$S$(SEP)list list$(SEP)$S)))

$(call log,LIST_TEST_VARIANTS)


.PHONY=all clean debug test help $(LIST_TEST_VARIANTS)

ALL_TARGETS=$(RELEASE_EXE) $(DEBUG_EXE) $(TEST_EXE)

# Tests may be specified with "test=" or "tests="
# for make args, with no target present.
# if so, don't do all targets, just the test (execute)
# target
ifdef test
	TESTS=$(TEST_SELECTOR)$(test)
	TARGET=test
endif
ifdef tests
	TESTS=$(TEST_SELECTOR)$(TEST_SUITELABEL)$(tests)
	TARGET=test
endif
ifndef TARGET
	TARGET=$(ALL_TARGETS)
endif


### BEGIN TARGETS

# by default make all three executables 
all: $(TARGET)

debug: $(DEBUG_EXE)

# run unit tests
# "make test"
# include "tests=label" or "params=--run_test=@label" on command line to pass down
# (if using tests= or params=, the "test" target is optional)
# ("test" will always execute tests, making exe if needed)
test: | $(TEST_EXE)
	$(TEST_CMD) $(params) $(TESTS)

$(LIST_TEST_VARIANTS) : | $(TEST_EXE)
	$(TEST_EXE) $(TEST_LIST)

#### Commands and flags used

MKDIR_P = mkdir -p
CXX = clang++
CC  = clang

CXXFLAGS += -Wall -I. -U_FORTIFY_SOURCE
CFLAGS   += -Wall -I. -U_FORTIFY_SOURCE

DEBUG_FLAGS := -g -O0

GEN_DEPS_FLAGS = -MT $@ -MMD -MP -MF $(DEPENDENCIESDIR)/$*.d

# determine final linker (guess it from MAIN_SRC)
FINAL_LINKER =$(CXX) $(CXXFLAGS)
ifneq ($(IS_C_MAIN),)
	FINAL_LINKER =$(CC) $(CFLAGS)
endif

$(call log,FINAL_LINKER)

#### (end) commands and flags


### DIRECTORIES FOR INTERMEDIATE PRODUCTS (.o, .d)
###
### WARNING - these directions are DESTROYED by "make clean"
###
# all .d (dependency) files live here
DEPENDENCIESDIR=deps
# normal .o files
OBJDIR=obj
# .o files unique to tests
TSTOBJDIR=obj.test
# .o files compiled with DEBUG_FLAGS
DBGOBJDIR=obj.debug

INTERMEDIATE_PRODUCT_DIRS=${DEPENDENCIESDIR} ${OBJDIR} ${TSTOBJDIR} ${DBGOBJDIR}

## Late target; internal use only
$(INTERMEDIATE_PRODUCT_DIRS):
	${MKDIR_P} $@

#### (end subdirectories)


#### compilation rules

# release
$(OBJDIR)/%.o : %.cpp | $(INTERMEDIATE_PRODUCT_DIRS)
	$(CXX) $(CXXFLAGS) $(GEN_DEPS_FLAGS) -c $< -o $@

$(OBJDIR)/%.o : %.c  | $(INTERMEDIATE_PRODUCT_DIRS)
	$(CC) $(CFLAGS) $(GEN_DEPS_FLAGS) -c $< -o $@

# debug (and test)
$(DBGOBJDIR)/%.o $(TSTOBJDIR)/%.o : %.cpp | $(INTERMEDIATE_PRODUCT_DIRS)
	$(CXX) $(CXXFLAGS) $(GEN_DEPS_FLAGS) $(DEBUG_FLAGS) -c $< -o $@

$(DBGOBJDIR)/%.o $(TSTOBJDIR)/%.o : %.c | $(INTERMEDIATE_PRODUCT_DIRS)
	$(CC) $(CFLAGS) $(GEN_DEPS_FLAGS) $(DEBUG_FLAGS) -c $< -o $@

#### (end) compilation rules


### List of objects generated from list of .cpp files

$(call log,SOURCE_DIRS)
$(call log,EXCLUDE_SRC)

vpath % $(SOURCE_DIRS)
C_SRC_WDOT =$(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.c))
C_SRC_NODOT=$(C_SRC_WDOT:./%=%)
C_SRC      =$(filter-out $(EXCLUDE_SRC),$(C_SRC_NODOT))

CPP_SRC_WDOT =$(foreach dir,$(SOURCE_DIRS),$(wildcard $(dir)/*.cpp))
CPP_SRC_NODOT=$(CPP_SRC_WDOT:./%=%)
CPP_SRC      =$(filter-out $(EXCLUDE_SRC),$(CPP_SRC_NODOT))

$(call log,C_SRC_WDOT)
$(call log,C_SRC_NODOT)
$(call log,C_SRC)


$(call log,CPP_SRC_WDOT)
$(call log,CPP_SRC_NODOT)
$(call log,CPP_SRC)

# remove the locations of main(), for both test and release

C_SRC_TEST=$(filter-out %.cpp,$(TEST_SRC))
C_SRC_MAIN=$(filter-out %.cpp,$(MAIN_SRC))

C_SRC_NO_MAIN=$(filter-out $(C_SRC_MAIN) $(C_SRC_TEST),$(C_SRC))

$(call log,C_SRC_TEST)
$(call log,C_SRC_MAIN)
$(call log,C_SRC_NO_MAIN)

CPP_SRC_MAIN=$(filter %.cpp,$(MAIN_SRC))
CPP_SRC_TEST=$(filter %.cpp,$(TEST_SRC))

CPP_SRC_NO_MAIN=$(filter-out $(CPP_SRC_MAIN) $(CPP_SRC_TEST),$(CPP_SRC))

$(call log,CPP_SRC_MAIN)
$(call log,CPP_SRC_TEST)
$(call log,CPP_SRC_NO_MAIN)

# strip dirs; use for .o/.d targets only
C_SRC_FNAME  =$(foreach src,$(C_SRC_NO_MAIN),$(notdir $(src)))
CPP_SRC_FNAME=$(foreach src,$(CPP_SRC_NO_MAIN),$(notdir $(src)))

$(call log,C_SRC_FNAME)
$(call log,CPP_SRC_FNAME)


#### objects for the two main() files, three versions (C & C++)
MAIN_OBJ    = $(MAIN_SRC:%.cpp=$(OBJDIR)/$(notdir %.o))
MAIN_DBG_OBJ= $(MAIN_SRC:%.cpp=$(DBGOBJDIR)/$(notdir %.o))
TEST_OBJ    = $(TEST_SRC:%.cpp=$(TSTOBJDIR)/%.o)

$(call log,MAIN_OBJ)
$(call log,MAIN_DBG_OBJ)
$(call log,TEST_OBJ)


### NOW ALL THE PROCESSING FOR THE OTHER FILES (not main/test)


### Convert "filename only" (no dirs) to the correct .o folder/file
### "COMMON" means all files that don't have main()
COMMON_OBJ_C  =$(C_SRC_FNAME:%.c=$(OBJDIR)/%.o)
COMMON_OBJ_CPP=$(CPP_SRC_FNAME:%.cpp=$(OBJDIR)/%.o)
COMMON_OBJ    =$(COMMON_OBJ_C) $(COMMON_OBJ_CPP)

$(call log,COMMON_OBJ_C)
$(call log,COMMON_OBJ_CPP)
$(call log,COMMON_OBJ)

COMMON_DBGOBJ_C  =$(C_SRC_FNAME:%.c=$(DBGOBJDIR)/%.o)
COMMON_DBGOBJ_CPP=$(CPP_SRC_FNAME:%.cpp=$(DBGOBJDIR)/%.o)
COMMON_DBGOBJ    =$(COMMON_DBGOBJ_C) $(COMMON_DBGOBJ_CPP)


#### all objects that each target needs
# (this is where we include the appropriate main())
ALL_OBJ_FOR_MAIN  =$(MAIN_OBJ)     $(COMMON_OBJ)
ALL_OBJ_FOR_TESTS =$(TEST_OBJ)     $(COMMON_DBGOBJ) 
ALL_OBJ_FOR_DEBUG =$(MAIN_DBG_OBJ) $(COMMON_DBGOBJ) 

$(RELEASE_EXE): $(ALL_OBJ_FOR_MAIN)
	@echo ">>>>"
	$(FINAL_LINKER) -o $@ $^ $(LDFLAGS) $(LDLIBS) $(RELEASE_LDLIBS) 
	@echo "<<<<"

$(DEBUG_EXE): $(ALL_OBJ_FOR_DEBUG) 
	@echo ">>>>"
	$(FINAL_LINKER) $(DEBUG_FLAGS) -o $@ $^ $(LDFLAGS)  $(LDLIBS) $(RELEASE_LDLIBS)
	@echo "<<<<"

$(TEST_EXE): $(ALL_OBJ_FOR_TESTS)
	@echo ">>>>"
	$(FINAL_LINKER) $(DEBUG_FLAGS) -o $@ $^ $(LDFLAGS)  $(LDLIBS) $(TEST_LDLIBS) 
	@echo "<<<<"

clean:
	rm -rf $(RELEASE_EXE) $(TEST_EXE) $(DEBUG_EXE) $(INTERMEDIATE_PRODUCT_DIRS)

define HELP_TEXT
Makefile for C/C++ projects.

Basics:

Typing "make" is standard. This compiles the release, test, and debug executables.

It expects main() to be in main.c or main.cpp, and infers the language from that. It also infers the testing framework (Boost.test for C++, munit for C) from the language, and expects the test driver to be in tests.c or tests.cpp.

* Compiles all .c/.cpp files in current directory.
* Source subdirectories can be set via SOURCE_DIRS
* Specific files can be excluded via EXCLUDE_SRC.
* Extra libraries and flags can be set.
* Keeps dependencies and objects in subdirectories.

Testing:

While "make tests" compiles the unit testing executable "./tests" (which can be executed directly), "make test" executes unit tests.

NOTE: do not add test framework to SOURCE_DIRS or LDLIBS. It is automatically added.

"make test=TEST_NAME" executes the particular test, and "make tests=TAG_NAME" executes the tagged/suite of tests. This works for both Boost.test and Munit.

"make list-tests" (and any variant of that) will list the tests according to the tests executable.

Standard targets:

all clean 

main: makes "./main" (release) executable

debug: makes "./main-debug" executable (all symbols)

Customization:

Primary customization for your project is expected between the "BEGIN/END USER CONFIGURATION" lines. Ideally, nothing else is necessary. If it becomes necessary, the author would appreciate knowing what change was necessary if it was not obvious and planned for.

Bug reports, questions, suggestions encouraged!

Munit: https://github.com/nemequ/munit

Boost.test: https://live.boost.org/doc/libs/1_81_0/libs/test/doc/html/index.html

Makefile: Version $(VERSION)
Author:   brucem@fullerton.edu / bjmckenz@gmail.com
License:  https://creativecommons.org/licenses/by-sa/3.0/
endef

export HELP_TEXT

help: 
	@echo "$$HELP_TEXT" | fold -s -60



### INCLUDE DEPENDENCIES

CPP_SRC_FNAME=$(foreach src,$(CPP_SRC),$(basename $(notdir $(src))))
C_SRC_FNAME  =$(foreach src,$(C_SRC),$(basename $(notdir $(src))))
ALL_SRC_FNAME=$(CPP_SRC_FNAME) $(C_SRC_FNAME)
ALL_DEPENDENCIES=$(ALL_SRC_FNAME:%=$(DEPENDENCIESDIR)/%.d)

$(call log,CPP_SRC_FNAME)
$(call log,C_SRC_FNAME)
$(call log,ALL_SRC_FNAME)
$(call log,ALL_DEPENDENCIES)

-include $(ALL_DEPENDENCIES)

