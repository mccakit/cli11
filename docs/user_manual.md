# cli11 — User Manual

A command line parser for C++26, built as a named module. This is a fork of
[CLI11](https://github.com/CLIUtils/CLI11) with a different naming convention, a
module-based build, and a handful of API changes described in
[Differences from CLI11](#differences-from-cli11).

If you already know CLI11, read [Naming conventions](#naming-conventions) and
[Differences from CLI11](#differences-from-cli11) first; everything else will
look familiar.

---

## Contents

- [Getting started](#getting-started)
- [Naming conventions](#naming-conventions)
- [Options](#options)
- [Flags](#flags)
- [Positionals](#positionals)
- [Validators](#validators)
- [Subcommands](#subcommands)
- [Option groups](#option-groups)
- [Configuration files](#configuration-files)
- [Environment variables](#environment-variables)
- [Help output](#help-output)
- [Errors](#errors)
- [Type support](#type-support)
- [Utilities](#utilities)
- [Differences from CLI11](#differences-from-cli11)

---

## Getting started

The library is a single module. There are no headers to include:

```cpp
import cli11;
```

A complete program:

```cpp
import std;
import cli11;

auto main(int argc, char **argv) -> int
{
    cli::app_t app{"A program that does something"};

    int parameter{0};
    app.add_option("-p,--parameter", parameter, "A parameter");

    try
    {
        app.parse(argc, argv);
    }
    catch (const cli::parse_error_t &e)
    {
        return app.exit(e);
    }

    std::println("Parameter value: {}", parameter);
    return 0;
}
```

Running it:

```console
$ ./a.out
Parameter value: 0

$ ./a.out -p 4
Parameter value: 4

$ ./a.out --help
A program that does something
Usage: ./a.out [OPTIONS]

Options:
  -h,--help                   Print this help message and exit
  -p,--parameter INT          A parameter
```

### The parse/exit pattern

`parse` throws on anything that should stop the program — a bad argument, a
missing required option, but also `--help` and `--version`, which are *successful*
early exits. `exit` sorts that out: it prints help to `stdout`, prints errors to
`stderr`, and returns the process exit code for each case.

```cpp
try
{
    app.parse(argc, argv);
}
catch (const cli::parse_error_t &e)
{
    return app.exit(e);
}
```

> **No `CLI11_PARSE` macro.** Upstream CLI11 wraps the above in a macro. Macros
> cannot be exported across module boundaries, so it does not exist here. Write
> the try/catch.

Catch `cli::parse_error_t`, not `cli::error_t`. The latter also covers mistakes
in how *you* built the parser — a duplicate option name, an impossible
requirement — which should crash loudly during development rather than be
reported to the user as a command line problem.

### Building

The module partitions must be compiled in dependency order. With CMake and a
compiler that supports C++26 modules:

```cmake
add_library(cli11)
target_sources(cli11
    PUBLIC FILE_SET CXX_MODULES FILES
        src/version.cpp    src/timer.cpp      src/encoding.cpp
        src/string_tools.cpp
        src/error.cpp      src/formatter_fwd.cpp  src/argv.cpp
        src/split.cpp      src/config_fwd.cpp     src/type_tools.cpp
        src/validators.cpp
        src/option.cpp
        src/app.cpp        src/extra_validators.cpp
        src/config.cpp     src/formatter.cpp
        src/cli11.cpp)
target_compile_features(cli11 PUBLIC cxx_std_26)
```

`cli11.cpp` is the primary module interface; it re-exports every partition, so
`import cli11;` gives you all of it.

---

## Naming conventions

Three rules cover the whole API.

**Types end in `_t`.** Classes, enums, and type aliases alike:

| | |
| --- | --- |
| `cli::app_t` | the parser, a subcommand, or an option group |
| `cli::option_t` | one option, flag, or positional |
| `cli::validator_t` | a composable check |
| `cli::formatter_t` | help output |
| `cli::config_base_t` | configuration file reading and writing |
| `cli::app_ptr_t` | `std::shared_ptr<app_t>` |
| `cli::option_ptr_t` | `std::unique_ptr<option_t>` |

**Everything else is snake_case and lowercase.** Functions, enumerators,
variables, and the pre-built validator objects:

```cpp
app.add_option("-f,--file", file)->check(cli::existing_file);
app.add_option("-n", n)->check(cli::range_t{1, 9});
opt->multi_option_policy(cli::multi_option_policy_t::take_last);
```

Note the distinction in that snippet: `existing_file` is an *object* — a ready-made
validator — so it has no suffix. `range_t` is a *class* you construct, so it does.

**Names that collide with keywords take a trailing underscore.** There are five:

| Where | Enumerator |
| --- | --- |
| `cli::multi_option_policy_t` | `reject` — renamed rather than suffixed, since `throw` had a good synonym |
| `cli::detail::classifier_t` | `short_`, `long_` |
| `cli::as_number_with_unit_t::options_t` | `default_mode` |
| `cli::option_already_added_t` | `requires_` |

You will rarely type any of these; `classifier_t` is internal, and the others
appear only in unusual code.

---

## Options

An option binds a command line name to one of your variables. The variable's
type decides how many values the option takes and how they are converted.

```cpp
std::string file;
app.add_option("-f,--file", file, "The file to read");
```

The name string may hold any number of names, comma-separated. One leading dash
makes a short name, two make a long name, and no dash makes a positional:

```cpp
app.add_option("-f,--file,--filename,file", name);
```

### The option pointer

Every `add_*` call returns `cli::option_t *`, which is how settings are chained:

```cpp
app.add_option("-f,--file", file, "The file to read")
    ->required()
    ->check(cli::existing_file);
```

Keep the pointer if you want to inspect the option after parsing:

```cpp
auto *opt = app.add_flag("--verbose");

app.parse(argc, argv);

if (*opt)
{
    std::println("--verbose given {} times", opt->count());
}
```

`operator bool` reports whether the option was used at all; `count()` reports how
many times.

### Common modifiers

| Modifier | Effect |
| --- | --- |
| `->required()` | The option must appear |
| `->expected(n)` | Take exactly `n` values; a negative `n` sets a minimum |
| `->expected(min, max)` | Take between `min` and `max` values |
| `->type_name(str)` | Replace the generated type name in help |
| `->description(str)` | Change the description |
| `->group(name)` | Which help section to list under; `""` hides it |
| `->check(v)` | Reject values failing a validator |
| `->transform(v)` | Rewrite values through a validator |
| `->each(f)` | Run `f` on each value, in order |
| `->default_val(v)` | Set a default, verifying it converts |
| `->default_str(s)` | Set the string shown as the default in help |
| `->capture_default_str()` | Take the bound variable's current value as the default |
| `->delimiter(c)` | Split a single value on `c`, so `--x=a,b,c` gives three |
| `->envname(name)` | Read from an environment variable when absent |
| `->needs(other)` | Require another option alongside this one |
| `->excludes(other)` | Forbid another option alongside this one |
| `->allow_extra_args()` | Absorb surplus values; default for vector options |
| `->configurable(false)` | Forbid setting from a configuration file |

### Vectors and repetition

Binding a container makes an option repeatable and unbounded:

```cpp
std::vector<int> values;
app.add_option("-v,--value", values);
```

```console
$ ./a.out -v 1 -v 2 -v 3
$ ./a.out -v 1 2 3
```

### Multiple values, one option

When an option appears more times than it expects values, the multi-option
policy decides what happens:

```cpp
opt->multi_option_policy(cli::multi_option_policy_t::take_last);
```

| Policy | Effect |
| --- | --- |
| `reject` | Report an error. The default. |
| `take_last` | Keep the last value |
| `take_first` | Keep the first value |
| `take_all` | Keep every value |
| `reverse` | Keep every value, last first |
| `join` | Join the values with the option's delimiter |
| `sum` | Add the values together |

There are shorthands for the common ones: `->take_last()`, `->take_first()`,
`->take_all()`, `->join()`, and `->join(delimiter)`.

> `reject` is spelled that way because upstream calls it `Throw`, and `throw` is
> a keyword.

### Inherited defaults

Settings applied to `option_defaults()` affect every option added *afterwards*:

```cpp
app.option_defaults()->ignore_case()->group("Required");

app.add_flag("--CaSeLeSs");   // matches --caseless, listed under "Required"
```

The inheritable settings are `group`, `required`, `multi_option_policy`,
`ignore_case`, `ignore_underscore`, `configurable`, `disable_flag_override`,
`always_capture_default`, `delimiter`, and `callback_priority`. Subcommands
inherit the defaults in force when they were created.

---

## Flags

A flag is an option that takes no value. Its meaning comes from being present.

```cpp
bool verbose{false};
app.add_flag("-v,--verbose", verbose, "Print more");
```

### Counting

Bind an integer wider than a byte and the flag counts instead of toggling:

```cpp
int verbosity{0};
app.add_flag("-v", verbosity);   // -vvv gives 3
```

This is not a special case in `add_flag`; it falls out of the type. An integral
type larger than one byte gets the `sum` multi-option policy and a default of
`0` automatically, so repetition accumulates rather than overwriting. A `char`,
a `bool`, an enum, or a string does not.

### Negation and explicit values

A flag can carry the value it means, in braces, and a leading `!` negates:

```cpp
bool flag{true};
app.add_flag("--flag,!--no-flag", flag);
```

```console
$ ./a.out --flag       # true
$ ./a.out --no-flag    # false
```

Braces set an arbitrary value, which is useful for enums and integers:

```cpp
int level{0};
app.add_flag("--low{1},--medium{5},--high{9}", level);
```

By default a user may also write `--flag=false` to override. If you would rather
the flag mean exactly one thing, `->disable_flag_override()` makes any other
value an error.

### Flags that call something

When there is no variable to bind, bind a function:

```cpp
app.add_flag_callback("--now", [] { std::println("{}", std::chrono::system_clock::now()); });

app.add_flag_function("-v", [](std::int64_t count) { set_verbosity(count); });
```

`add_flag_callback` runs on presence; `add_flag_function` receives the count, so
it sees `3` for `-vvv`.

### Collecting flag values

A flag bound to a vector records one entry per appearance, which preserves order
in a way a counter cannot:

```cpp
std::vector<int> levels;
app.add_flag("--low{1},--high{9}", levels);   // --low --high --low  ->  {1, 9, 1}
```

---

## Positionals

A name with no dashes is a positional:

```cpp
std::string input;
app.add_option("input", input, "The input file")->required();
```

Positionals fill in declaration order. A positional bound to a container is
unbounded and will absorb everything left, so at most one such positional can be
unambiguous — the parser rejects a configuration with two of them unless all but
one are required.

Two settings help when the boundary between options and positionals is unclear.

**`app.positionals_at_end()`** requires every positional to come after the last
option, so a value that looks like an option is never silently swallowed as a
positional.

**`app.validate_positionals()`** consults each positional's validators before
assigning a value. A positional that would reject the value is passed over and a
later one gets a chance:

```cpp
int count{};
std::string name;
app.add_option("count", count)->check(cli::number);
app.add_option("name", name);
app.validate_positionals();
```

```console
$ ./a.out hello 3     # count=3, name=hello — despite the order
```

Failing validation here is not an error; it only changes which positional is
tried. Anything that matches nothing ends up in the remaining-argument list.

The same idea applies to optional values with `app.validate_optional_arguments()`,
which stops an unbounded option from eating arguments meant for a positional:

```cpp
std::vector<std::string> files;
std::vector<int> ids;
app.add_option("files", files);
app.add_option("--id", ids)->check(cli::number);
app.validate_optional_arguments();
```

```console
$ ./a.out --id 1 2 3 alpha beta    # ids={1,2,3}, files={alpha,beta}
```

Without it, `--id` would consume `alpha` and fail to convert it.

---

## Validators

A validator inspects one value and either accepts it or explains why not. Attach
one with `->check(...)`:

```cpp
app.add_option("-f,--file", file)->check(cli::existing_file);
app.add_option("-n,--count", n)->check(cli::range_t{1, 10});
```

A validator's description becomes part of the option's type name in help output,
so `--count INT in [1 - 10]` is generated rather than written by hand.

### Built in

**Filesystem**

| | |
| --- | --- |
| `cli::existing_file` | Names an existing file |
| `cli::existing_directory` | Names an existing directory |
| `cli::existing_path` | Names anything that exists |
| `cli::nonexistent_path` | Names nothing that exists |
| `cli::read_permissions` | Exists and is readable |
| `cli::write_permissions` | Exists and is writable |
| `cli::exec_permissions` | Exists and is executable |
| `cli::non_empty_file` | Names a file of non-zero size |
| `cli::file_size_validator_t{min, max}` | Names a file within a size range |
| `cli::file_on_default_path_t{dir}` | Resolves against `dir` if not found as given |

**Numeric**

| | |
| --- | --- |
| `cli::number` | Parses as a number |
| `cli::positive_number` | Greater than zero |
| `cli::non_negative_number` | Zero or greater |
| `cli::range_t{min, max}` | Within a closed interval |
| `cli::bound_t{min, max}` | *Clamps* into the interval instead of rejecting |
| `cli::type_validator_t<T>{}` | Parses as `T` |

**Other**

| | |
| --- | --- |
| `cli::valid_ipv4` | A dotted IPv4 address |
| `cli::escaped_string` | Resolves quoting and backslash escapes in place |

Note `range_t` and `bound_t` differ in outcome, not in test: `range_t{1, 10}`
errors on `20`, `bound_t{1, 10}` rewrites it to `10`.

### Sets of allowed values

`is_member_t` restricts a value to a set, and the set can be anything iterable:

```cpp
std::vector<std::string> colors{"red", "green", "blue"};
app.add_option("--color", color)->check(cli::is_member_t{colors});
```

```cpp
app.add_option("--color", color)->check(cli::is_member_t{{"red", "green", "blue"}});
```

Pass a *pointer* to the set and it is re-read on every check, so a set that
changes after construction still validates correctly:

```cpp
auto colors = std::make_shared<std::set<std::string>>();
app.add_option("--color", color)->check(cli::is_member_t{colors});
colors->insert("violet");   // takes effect
```

Add filters to loosen matching. Each is applied to both sides of the comparison:

```cpp
app.add_option("--color", color)->check(cli::is_member_t{colors, cli::ignore_case});
app.add_option("--color", color)
   ->check(cli::is_member_t{colors, cli::ignore_case, cli::ignore_underscore});
```

The filters are `cli::ignore_case`, `cli::ignore_underscore`, and
`cli::ignore_space`, and any callable of the same shape works. On a filtered
match the value is rewritten to the spelling held in the set, so downstream code
sees the canonical form rather than whatever the user typed.

### Mapping values

`transformer_t` maps a value through a lookup table:

```cpp
std::map<std::string, int> levels{{"low", 1}, {"medium", 5}, {"high", 9}};
app.add_option("--level", level)->transform(cli::transformer_t{levels, cli::ignore_case});
```

`checked_transformer_t` is the same, except that a value matching neither a key
nor an already-mapped output is an error. That distinction matters when a
transform might run twice — a `checked_transformer_t` accepts its own output,
so it is idempotent.

Use `->transform(...)` rather than `->check(...)` for these: transforms are
prepended and run first, checks are appended and run after.

### Parsing units

`as_number_with_unit_t` multiplies a value by a factor drawn from a table:

```cpp
std::map<std::string, double> seconds{{"s", 1}, {"min", 60}, {"h", 3600}};
double duration{};
app.add_option("--wait", duration)->transform(cli::as_number_with_unit_t{seconds});
```

```console
$ ./a.out --wait 90s
$ ./a.out --wait 2h
$ ./a.out --wait 1.5 min
```

The result type follows the map's value type, so map to `double` if fractional
inputs should work.

Matching is case-insensitive and the unit is optional by default. Change either
with the options flag:

```cpp
using opts = cli::as_number_with_unit_t::options_t;
cli::as_number_with_unit_t{seconds, opts::case_sensitive | opts::unit_required};
```

`as_size_value_t` is a ready-made instance of this for byte sizes:

```cpp
std::uint64_t size{};
app.add_option("--size", size)->transform(cli::as_size_value_t{true});
```

```console
$ ./a.out --size 100      # 100
$ ./a.out --size 10kb     # 10000, because kb_is_1000 is true
$ ./a.out --size 10kib    # 10240, always a power of two
```

The constructor argument decides whether the plain spellings (`kb`, `mb`) mean
powers of 1000 or of 1024. The `*i` and `*ib` spellings always mean 1024.

### Composing

Validators combine with `&`, `|`, and `!`, and the descriptions combine to match:

```cpp
app.add_option("--path", path)->check(cli::existing_file | cli::existing_directory);
app.add_option("--out", out)->check(!cli::existing_path);
app.add_option("-n", n)->check(cli::positive_number & !cli::range_t{5, 10});
```

### Writing your own

Anything callable that takes `std::string &` and returns a message — empty for
success — is a validator:

```cpp
app.add_option("--even", n)->check(
    [](const std::string &value) {
        return (std::stoi(value) % 2 == 0) ? std::string{} : "must be even";
    },
    "EVEN");
```

The second argument is the description shown in help. For something reusable,
construct a `cli::validator_t` directly and name it, which lets it be found
later with `opt->get_validator("name")`:

```cpp
const cli::validator_t even{
    [](std::string &value) {
        return (std::stoi(value) % 2 == 0) ? std::string{} : "must be even";
    },
    "EVEN", "even_check"};
```

A validator taking `std::string &` may rewrite its argument — that is what makes
`->transform()` work. `->check()` marks the validator non-modifying, so the same
object behaves as a pure test when used that way.

---

## Subcommands

A subcommand is a full `app_t`. Everything above works on one.

```cpp
cli::app_t app{"Version control"};
app.require_subcommand(1);

auto *commit = app.add_subcommand("commit", "Record changes");
std::string message;
commit->add_option("-m,--message", message)->required();

auto *push = app.add_subcommand("push", "Upload changes");
bool force{false};
push->add_flag("-f,--force", force);
```

After parsing, ask which ran:

```cpp
if (*commit)          { do_commit(message); }
if (app.got_subcommand("push")) { do_push(force); }
```

Or hang the work off a callback, which runs when that subcommand completes:

```cpp
commit->callback([&] { do_commit(message); });
```

Callbacks fire bottom-up: the deepest subcommand first, its parent after. With
`->immediate_callback()` a subcommand's callback runs as soon as it finishes
parsing instead, which matters when the same subcommand can appear twice and
each occurrence should be handled separately.

### How many

```cpp
app.require_subcommand();          // at least one
app.require_subcommand(1);         // exactly one
app.require_subcommand(-2);        // at most two
app.require_subcommand(1, 3);      // between one and three
```

The maximum also stops parsing: once that many subcommands have been seen, later
names are treated as ordinary arguments rather than as subcommands.

### Fallthrough

By default an option written after a subcommand name belongs to that subcommand.
`->fallthrough()` lets an unmatched one reach the parent instead:

```cpp
auto *sub = app.add_subcommand("build")->fallthrough();
app.add_flag("--verbose");
```

```console
$ ./a.out build --verbose     # --verbose matches on the parent
```

A side effect worth knowing: help for a subcommand with fallthrough lists the
parent's options too, since they are genuinely usable there.

Separately, `->subcommand_fallthrough(false)` stops *further* subcommands at the
same level from being recognised once this one is active, so their names become
positional values instead.

### Nesting and dotted names

Subcommands nest without limit, and a nested one can be addressed directly:

```console
$ ./a.out remote add --url=...
$ ./a.out remote.add --url=...
$ ./a.out --remote.add.url=...
```

The dotted forms are equivalent to naming each level in turn.

### Prefix commands

`->prefix_command()` stops parsing at the first unrecognised argument and hands
everything from there on to you. This is how `git` dispatches to `git-thing`:

```cpp
app.prefix_command();
app.parse(argc, argv);
auto rest = app.remaining_for_passthrough();
```

`prefix_command_mode_t::separator_only` is a stricter variant: only an explicit
`--` starts the passthrough, and anything else unrecognised is still an error.

### Other modifiers

| | |
| --- | --- |
| `->required()` | The subcommand must be used |
| `->disabled()` | Not matched, not shown |
| `->silent()` | Works, but stays out of `get_subcommands()` |
| `->ignore_case()` | Match the name case-insensitively |
| `->ignore_underscore()` | Match ignoring underscores |
| `->allow_subcommand_prefix_matching()` | `upg` matches `upgrade`, if unambiguous |
| `->alias(name)` | Answer to another name as well |

`silent()` turns a subcommand into a modifier — it does its work without
appearing in the list of what ran:

```cpp
auto *help_cmd = app.add_subcommand("help")->silent();
help_cmd->parse_complete_callback([] { throw cli::call_for_help_t(); });
```

```console
$ ./a.out help
$ ./a.out help build
```

### Triggering

One subcommand can enable or disable others when it is used:

```cpp
cli::trigger_on(setup, {network, storage});   // setup enables these
cli::trigger_off(offline, network);           // offline disables this
```

---

## Option groups

An option group is a subcommand with no name. It never appears on the command
line; it exists to group options in help and to carry requirements of its own.

```cpp
auto *format = app.add_option_group("Format", "Output format");
format->add_flag("--json");
format->add_flag("--xml");
format->add_flag("--yaml");
format->require_option(1);        // exactly one of the three
```

`require_option` takes the same shapes as `require_subcommand`:

```cpp
group->require_option();          // at least one
group->require_option(1);         // exactly one
group->require_option(-2);        // at most two
group->require_option(1, 3);      // between one and three
```

Move an existing option into a group rather than creating it there:

```cpp
auto *opt = app.add_option("--width", width);
group->add_option(opt);
```

A group whose name begins with `+` is merged into the parent's help output
instead of being listed separately.

---

## Configuration files

```cpp
app.set_config("--config");
```

That adds an option naming a file to read. Values from the file are applied as
though they had been typed, so validators, transforms, and callbacks all run
normally. Command line arguments take precedence over the file.

The remaining parameters set a default filename, the help text, and whether the
file is required:

```cpp
app.set_config("--config", "defaults.toml", "Read a configuration file", true);
```

`set_config` returns an ordinary option pointer, so it takes modifiers like any
other:

```cpp
app.set_config("--config")->transform(cli::file_on_default_path_t{"/etc/myapp/"});
```

Naming the option more than once reads each file in turn, later ones winning.

### Format

The default is TOML-shaped. Sections map to subcommands:

```toml
verbose = true
level = 5
files = ["a.txt", "b.txt"]

[build]
target = "release"
jobs = 8

[build.cache]
enabled = true
```

`[build.cache]` addresses subcommand `cache` of subcommand `build`. A subcommand
only gets its own section if it is `->configurable()`; otherwise its options are
written with a dotted prefix instead.

For INI output, swap the converter:

```cpp
app.config_formatter(std::make_shared<cli::config_ini_t>());
```

Both are `config_base_t` with different punctuation, and every character is a
setting, so you can build a third dialect without writing a parser:

```cpp
auto fmt = std::make_shared<cli::config_base_t>();
fmt->comment('#')->array_bounds('[', ']')->array_delimiter(',')
   ->value_separator('=')->parent_separator('.');
app.config_formatter(fmt);
```

Other settings on `config_base_t`: `quote_character`, `max_layers`,
`comment_defaults`, `allow_duplicate_fields`, `section`, and `index` — the last
two select a single section, or a single element of an arrayed section, from a
larger file.

### Writing one out

```cpp
std::println("{}", app.config_to_str(cli::config_output_mode_t::all_defaults, true));
```

| Mode | Writes |
| --- | --- |
| `active` | Only options that were actually set |
| `all_defaults` | Everything, including untouched defaults |
| `active_subcommand_defaults` | Defaults, but only for subcommands that were used |

The second argument includes descriptions as comments.

### Entries that match nothing

```cpp
app.allow_config_extras(cli::config_extras_mode_t::ignore);
```

| Mode | Effect |
| --- | --- |
| `error` | Report an error |
| `ignore` | Skip the entry. The default. |
| `ignore_all` | Skip it and anything nested under it |
| `capture` | Collect it for you to inspect |

`capture` also sets `allow_extras`, since a captured entry has to go somewhere.

---

## Environment variables

```cpp
app.add_option("--key", key)->envname("MY_APP_KEY");
```

The variable is consulted only when the option is absent from the command line,
and the value goes through the option's validators. A value that fails them is
ignored rather than reported, so a stale environment variable cannot break a run
that did not depend on it.

---

## Help output

`-h,--help` is added automatically. Replace or remove it:

```cpp
app.set_help_flag("-?,--help", "Show this message");
app.set_help_flag("");                                     // remove
app.set_help_all_flag("--help-all", "Show expanded help"); // include subcommands
```

Add a version flag:

```cpp
app.set_version_flag("-V,--version", "1.2.0");
app.set_version_flag("-V,--version", [] { return build_version_string(); });
```

### Layout

```cpp
app.get_formatter()->column_width(40);
app.get_formatter()->right_column_width(60);
app.get_formatter()->description_paragraph_width(100);
```

| | |
| --- | --- |
| `column_width` | Width of the left column |
| `right_column_width` | Width of the description column |
| `long_option_alignment_ratio` | Where long names start within the left column |
| `description_paragraph_width` | Wrapping width for the description |
| `footer_paragraph_width` | Wrapping width for the footer |
| `enable_description_formatting` | Whether the description is reflowed |
| `enable_option_defaults` | Whether defaults are printed |
| `enable_option_type_names` | Whether type names are printed |
| `enable_default_flag_values` | Whether `{value}` suffixes are printed |

Section labels are overridable, which is the simplest route to another language:

```cpp
app.get_formatter()->label("OPTIONS", "Optionen");
app.get_formatter()->label("REQUIRED", "erforderlich");
```

Surrounding text:

```cpp
app.description("What this program does");
app.usage("Usage: prog <command> [options] <file>");
app.footer("See the manual for details.");
```

`usage` and `footer` also accept a callable, for text that is only known at
print time.

### Replacing the formatter

For a small change, wrap a lambda:

```cpp
app.formatter_fn([](const cli::app_t *a, std::string name, cli::app_format_mode_t) {
    return "usage: " + name + "\n";
});
```

For a real one, derive from `cli::formatter_t` and override the piece you care
about. The page is assembled from separate virtuals — `make_option`,
`make_group`, `make_positionals`, `make_subcommands`, `make_description`,
`make_usage`, `make_footer`, and `make_help` on top — so overriding one leaves
the rest intact:

```cpp
class my_formatter_t : public cli::formatter_t
{
    public:
        [[nodiscard]] auto make_option_opts(const cli::option_t *) const -> std::string override
        {
            return {};   // suppress type names and defaults entirely
        }
};

app.formatter(std::make_shared<my_formatter_t>());
```

---

## Errors

Everything thrown derives from `cli::error_t`, which derives from
`std::runtime_error`. The hierarchy splits in two.

**`cli::construction_error_t`** means the parser was built wrong — a duplicate
name, a flag declared positional, an impossible requirement. These are your
bugs, thrown from `add_option` and friends, and should surface loudly during
development.

**`cli::parse_error_t`** means the command line was wrong, or that parsing
should stop early. This is the one to catch:

```cpp
try
{
    app.parse(argc, argv);
}
catch (const cli::parse_error_t &e)
{
    return app.exit(e);
}
```

`--help` and `--version` arrive as `parse_error_t` too — as
`cli::call_for_help_t`, `cli::call_for_all_help_t`, and
`cli::call_for_version_t`, all deriving from `cli::success_t`. They are
successful early exits, and `exit` prints them to `stdout` with a zero code
rather than treating them as failures. This is why you catch `parse_error_t`
rather than picking off individual types.

The ones you might name explicitly:

| Type | Meaning |
| --- | --- |
| `cli::required_error_t` | A required option or subcommand was absent |
| `cli::conversion_error_t` | A value would not convert to its target type |
| `cli::validation_error_t` | A value failed a validator |
| `cli::argument_mismatch_t` | Wrong number of values |
| `cli::excludes_error_t` | Two mutually exclusive options were used |
| `cli::requires_error_t` | An option was used without one it depends on |
| `cli::extras_error_t` | Arguments were left over |
| `cli::config_error_t` | A configuration file could not be applied |
| `cli::file_error_t` | A file could not be read |

Each carries a name and an exit code:

```cpp
catch (const cli::parse_error_t &e)
{
    std::println(stderr, "{}: {}", e.get_name(), e.what());
    return e.get_exit_code();
}
```

`get_name()` returns the type's own name — `"validation_error"`,
`"required_error"` — and each class exposes it as a constant, so a comparison
need not hardcode a string:

```cpp
if (e.get_name() == cli::call_for_help_t::error_type_name) { /* ... */ }
```

Exit codes come from `cli::exit_codes_t`, with `success` at 0 and the rest from
100 up.

### Changing the message

```cpp
app.failure_message(cli::failure_message::help);   // print full help on error
```

`cli::failure_message::simple` is the default — the message plus a pointer to
`--help`. Supply your own for anything else:

```cpp
app.failure_message([](const cli::app_t *a, const cli::error_t &e) {
    return std::format("error: {}\ntry '{} --help'\n", e.what(), a->get_name());
});
```

### Leftover arguments

With `app.allow_extras()`, unmatched arguments are collected instead of
reported:

```cpp
app.allow_extras();
app.parse(argc, argv);

for (const auto &arg : app.remaining())        { /* this command only */ }
for (const auto &arg : app.remaining(true))    { /* including subcommands */ }
auto passthrough = app.remaining_for_passthrough();   // command line order
```

`remaining()` gives them in reverse parse order; `remaining_for_passthrough()`
gives them the way the user typed them, ready to hand to another program.

`cli::extras_mode_t` offers finer control than the boolean:

| Mode | Effect |
| --- | --- |
| `error` | Report at the end. The default. |
| `error_immediately` | Report as soon as one appears |
| `ignore` | Discard silently |
| `capture` | Collect for inspection |
| `assume_single_argument` | Treat the next argument as this option's value |
| `assume_multiple_arguments` | Treat every following argument as this option's values |

---

## Type support

`add_option` works out what a type means rather than requiring you to say. The
classification lives in `cli::detail::object_category_t` and decides three
things: how many values the option consumes, how they convert, and what type
name appears in help.

You never name these categories in ordinary use. They matter when a type does
not do what you expected.

| Your type | Consumes | Shown as |
| --- | --- | --- |
| `int`, `long`, `std::size_t` | 1 | `INT`, `UINT` |
| `double`, `float` | 1 | `FLOAT` |
| `bool` | 1 | `BOOLEAN` |
| `char` | 1 | `CHAR` |
| an enum | 1 | `ENUM` |
| `std::string` | 1 | `TEXT` |
| `std::complex<T>` | 2 | `COMPLEX` |
| `std::pair<A, B>` | 2 | `[A,B]` |
| `std::tuple<A, B, C>` | 3 | `[A,B,C]` |
| `std::optional<T>` | as `T` | as `T` |
| `std::vector<T>` | unbounded | as `T` |
| `std::vector<std::pair<A, B>>` | pairs, unbounded | `[A,B]` |
| `std::map<K, V>` | pairs, unbounded | `[K,V]` |

Composition works as you would expect: `std::vector<std::tuple<int, std::string,
double>>` consumes three values per entry and repeats.

### Integer notation

Integer conversion accepts more than decimal:

```console
$ ./a.out -n 42
$ ./a.out -n 0x2a        # hex
$ ./a.out -n 0o52        # octal
$ ./a.out -n 0b101010    # binary
$ ./a.out -n 1_000_000   # digit separators, also ' and the locale's separator
```

### Reading as one type, storing as another

Give `add_option` two types when the command line form differs from the stored
form:

```cpp
std::atomic<int> count;
app.add_option<std::atomic<int>, int>("--count", count);
```

The value is parsed as the second type and assigned to the first.

### Teaching it a new type

If a type has `operator>>`, it already works — that is the fallback. Otherwise
provide a `lexical_cast` findable by ADL in the type's own namespace:

```cpp
namespace geometry
{
    struct point_t
    {
            double x{};
            double y{};
    };

    auto lexical_cast(const std::string &input, point_t &output) -> bool
    {
        const auto comma = input.find(',');
        if (comma == std::string::npos) { return false; }
        return cli::detail::lexical_cast(input.substr(0, comma), output.x) &&
               cli::detail::lexical_cast(input.substr(comma + 1), output.y);
    }
}

geometry::point_t origin;
app.add_option("--origin", origin);
```

A type with no conversion route at all fails at compile time with a message
saying so, rather than as an overload resolution error.

### Constraining your own templates

The classification is exposed as concepts, so code that must branch on category
can:

```cpp
template <cli::detail::integral_value_like T> auto describe() -> std::string { return "an integer"; }
template <cli::detail::container_value_like T> auto describe() -> std::string { return "a container"; }
```

There is one concept per category — `char_value_like`, `integral_value_like`,
`unsigned_integral_like`, `enumeration_like`, `boolean_value_like`,
`floating_point_like`, `complex_number_like`, `tuple_value_like`,
`container_value_like`, `wrapper_value_like`, `text_like`, and the
constructible-from variants.

---

## Utilities

### Timing

```cpp
{
    const cli::auto_timer_t t{"parse"};
    app.parse(argc, argv);
}   // prints "parse: 1.2345 ms"
```

`cli::timer_t` is the manual form:

```cpp
cli::timer_t timer{"work", cli::timer_t::big};
do_work();
std::println("{}", timer.to_string());
```

`simple` renders `title: time`; `big` draws a banner. Time a repeated operation
with `time_it`, which runs the callable until a target duration has elapsed and
reports the mean:

```cpp
cli::timer_t timer;
std::println("{}", timer.time_it([] { do_work(); }, 1.0));
```

### Encoding

On Windows, `argv` has already been narrowed to the active code page, losing any
character it cannot represent. `ensure_utf8` goes back to the real command line:

```cpp
auto main(int argc, char **argv) -> int
{
    cli::app_t app{"..."};
    argv = app.ensure_utf8(argv);
    // ...
}
```

It does nothing elsewhere, so it is safe to call unconditionally. The returned
pointer is owned by the application.

The conversions are available directly:

```cpp
std::string  s = cli::narrow(L"wide");
std::wstring w = cli::widen("narrow");
std::filesystem::path p = cli::to_path("some/path");
```

Use `cli::to_path` rather than constructing a `std::filesystem::path` yourself
when the string came from the command line; on Windows it widens first, so paths
outside the code page survive.

> The `std::string_view` overloads of `narrow` and `widen` read to a null
> terminator. A view over a substring is not null-terminated, so pass a whole
> `std::string` or a null-terminated buffer to those two.

### Whole command lines

Parse a command line held in one string, rather than an argument array:

```cpp
app.parse("--file input.txt --verbose");
app.parse("myprog --file input.txt", true);   // true: the program name is included
```

Quoting and escapes are handled, so `--message="hello world"` arrives as one
value.

Or parse a stream as a configuration file:

```cpp
std::ifstream input{"settings.toml"};
app.parse_from_stream(input);
```

### Reuse

```cpp
app.parse(first_command_line);
app.clear();
app.parse(second_command_line);
```

`clear` discards collected values and resets the parse state. `parse` calls it
for you if the application has already been used.

---

## Differences from CLI11

This fork is API-incompatible with upstream CLI11. There are no compatibility
aliases; porting is a mechanical rename plus the handful of real changes below.

### Names

The namespace is `cli`. Types gain `_t`, everything else is snake_case.

| CLI11 | here |
| --- | --- |
| `CLI::App` | `cli::app_t` |
| `CLI::Option` | `cli::option_t` |
| `CLI::Option_group` | `cli::option_group_t` |
| `CLI::OptionDefaults` | `cli::option_defaults_t` |
| `CLI::Validator` | `cli::validator_t` |
| `CLI::Formatter`, `CLI::FormatterBase` | `cli::formatter_t`, `cli::formatter_base_t` |
| `CLI::Config`, `CLI::ConfigBase` | `cli::config_t`, `cli::config_base_t` |
| `CLI::ConfigTOML`, `CLI::ConfigINI` | `cli::config_toml_t`, `cli::config_ini_t` |
| `CLI::ConfigItem` | `cli::config_item_t` |
| `CLI::Timer`, `CLI::AutoTimer` | `cli::timer_t`, `cli::auto_timer_t` |
| `CLI::App_p`, `CLI::Option_p` | `cli::app_ptr_t`, `cli::option_ptr_t` |
| `CLI::Error`, `CLI::ParseError` | `cli::error_t`, `cli::parse_error_t` |
| `CLI::ExitCodes` | `cli::exit_codes_t` |
| `CLI::MultiOptionPolicy` | `cli::multi_option_policy_t` |
| `CLI::CallbackPriority` | `cli::callback_priority_t` |
| `CLI::AppFormatMode` | `cli::app_format_mode_t` |
| `CLI::ExtrasMode` | `cli::extras_mode_t` |
| `CLI::PrefixCommandMode` | `cli::prefix_command_mode_t` |

Validator objects lose PascalCase but gain no suffix, since they are values:

| CLI11 | here |
| --- | --- |
| `CLI::ExistingFile` | `cli::existing_file` |
| `CLI::ExistingDirectory` | `cli::existing_directory` |
| `CLI::ExistingPath` | `cli::existing_path` |
| `CLI::NonexistentPath` | `cli::nonexistent_path` |
| `CLI::EscapedString` | `cli::escaped_string` |
| `CLI::PositiveNumber` | `cli::positive_number` |
| `CLI::NonNegativeNumber` | `cli::non_negative_number` |
| `CLI::Number` | `cli::number` |
| `CLI::ValidIPV4` | `cli::valid_ipv4` |

Validator *classes* keep the suffix:

| CLI11 | here |
| --- | --- |
| `CLI::Range` | `cli::range_t` |
| `CLI::Bound` | `cli::bound_t` |
| `CLI::IsMember` | `cli::is_member_t` |
| `CLI::Transformer` | `cli::transformer_t` |
| `CLI::CheckedTransformer` | `cli::checked_transformer_t` |
| `CLI::AsNumberWithUnit` | `cli::as_number_with_unit_t` |
| `CLI::AsSizeValue` | `cli::as_size_value_t` |
| `CLI::FileOnDefaultPath` | `cli::file_on_default_path_t` |
| `CLI::TypeValidator` | `cli::type_validator_t` |

Enumerators are lowercase: `MultiOptionPolicy::TakeLast` becomes
`multi_option_policy_t::take_last`, `ExtrasMode::Capture` becomes
`extras_mode_t::capture`, and so on. Free functions `TriggerOn` and `TriggerOff`
become `trigger_on` and `trigger_off`.

Five names collided with keywords once lowercased and carry a trailing
underscore, except the first, which had a usable synonym:

| CLI11 | here |
| --- | --- |
| `MultiOptionPolicy::Throw` | `multi_option_policy_t::reject` |
| `Classifier::SHORT`, `::LONG` | `classifier_t::short_`, `::long_` |
| `AsNumberWithUnit::Options::DEFAULT` | `options_t::default_mode` |
| `OptionAlreadyAdded::Requires` | `option_already_added_t::requires_` |

### `CLI11_PARSE` is gone

Macros are not exported across module boundaries. Write the try/catch:

```cpp
try                                          // CLI11_PARSE(app, argc, argv);
{
    app.parse(argc, argv);
}
catch (const cli::parse_error_t &e)
{
    return app.exit(e);
}
```

### Behavioural changes

These are not renames. Code that compiles after the rename may still be wrong.

**`get_name()` returns different strings.** An error's name now matches its type:
`"validation_error"` rather than `"ValidationError"`. Anything comparing against
the old strings will silently stop matching. Compare against the constant
instead:

```cpp
if (e.get_name() == cli::call_for_help_t::error_type_name) { /* ... */ }
```

**The two config-extras enumerations were merged.** Upstream carried both
`ConfigExtrasMode` and `config_extras_mode` with identical members, plus an
`allow_config_extras` overload for each. There is now one
`cli::config_extras_mode_t` and one overload. Calls using either spelling
compile unchanged.

**`allow_config_extras(true)` still also sets `allow_extras`.** Unchanged from
upstream, but easy to forget: `config_extras_mode_t::capture` does the same,
while the other modes do not.

**Several internal functions changed shape.** These are in `cli::detail` and
unlikely to appear in application code, but they are breaking for anything that
reached into them:

| Was | Now |
| --- | --- |
| `split_short(s, name, rest) -> bool` | `split_short(s) -> std::optional<split_result_t>` |
| `split_long(s, name, value) -> bool` | `split_long(s) -> std::optional<split_result_t>` |
| `split_windows_style(s, name, value) -> bool` | `split_windows_style(s) -> std::optional<split_result_t>` |
| `get_names(...) -> std::tuple<vector, vector, string>` | `-> option_names_t{short_names, long_names, positional_name}` |
| `split_program_name(s) -> std::pair<string, string>` | `-> program_name_t{name, arguments}` |
| `find_member(...) -> std::ptrdiff_t`, `-1` for absent | `-> std::optional<std::size_t>` |
| `check_path(const char *)` | `check_path(std::string_view)` |

**Accessors that returned by value now return by reference.**
`option_t::get_needs`, `get_excludes`, `get_callback`, `get_envname`,
`get_default_str`; `app_t::get_subcommands()` and `get_description`. Binding the
result to `auto` now copies where it previously copied anyway, but binding to
`auto &` is newly possible and worth doing.

**The SFINAE machinery is gone.** `CLI::enable_if_t`, `CLI::void_t`,
`CLI::conditional_t`, `detail::enabler`, and `detail::dummy` were aliases for
standard facilities or scaffolding for `std::enable_if`; dispatch is by concept
now. The detection traits are concepts too — `is_mutable_container<T>::value`
becomes `mutable_container<T>`, `is_tuple_like<T>::value` becomes `tuple_like<T>`,
and so on. The value-computing traits keep their form but gained `_v` aliases:
`type_count_v<T>`, `classify_object_v<T>`, `expected_count_v<T>`.

**`IsMemberType` is `is_member_type_t`**, and `object_category` is
`object_category_t`.

### Known issues

Two pre-existing bugs were found during the port and left in place, because
fixing them changes runtime behaviour. Both are marked `NOTE` in the source.

**`app_t::get_options` disagrees with itself.** The const overload descends into
a subcommand when it is nameless **and** its group starts with `+`; the
non-const overload descends when it is nameless **or** its group starts with `+`.
The two therefore return different option sets for the same application.

**`app_t::check_name_detail` drops underscore stripping.** With both
`ignore_case()` and `ignore_underscore()` enabled, the second transformation
re-reads the original name instead of the already-stripped one, so the candidate
gets both transformations and the subcommand's own name gets only the last. A
subcommand named `sub_command` will not match `subcommand` under that
combination.

### Requirements

C++26, and a standard library shipping `import std;`. The library uses `std::format`,
`std::ranges` including `ranges::to` and `ranges::contains`, `std::erase`/`std::erase_if`,
deducing `this`, concepts throughout, and `static_assert(false)` in uninstantiated
templates.

`std::copyable_function` would be the right type for the library's stored
callables — it fixes the const-correctness hole in `std::function` — but no
implementation ships it yet, so `std::function` is used with a note at each site.
