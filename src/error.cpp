export module cli11:error;

import std;
import :string_tools;

export namespace CLI
{

    enum class ExitCodes : int
    {
        Success = 0,
        IncorrectConstruction = 100,
        BadNameString,
        OptionAlreadyAdded,
        FileError,
        ConversionError,
        ValidationError,
        RequiredError,
        RequiresError,
        ExcludesError,
        ExtrasError,
        ConfigError,
        InvalidError,
        HorribleError,
        OptionNotFound,
        ArgumentMismatch,
        BaseClass = 127
    };

    class Error : public std::runtime_error
    {
            int actual_exit_code;
            std::string error_name {"Error"};

        public:
            [[nodiscard]] int get_exit_code() const
            {
                return actual_exit_code;
            }
            [[nodiscard]] std::string get_name() const
            {
                return error_name;
            }

            Error(std::string name, std::string msg, int exit_code = static_cast<int>(ExitCodes::BaseClass))
                : runtime_error(msg), actual_exit_code(exit_code), error_name(std::move(name))
            {
            }

            Error(std::string name, std::string msg, ExitCodes exit_code)
                : Error(name, msg, static_cast<int>(exit_code))
            {
            }
    };

    class ConstructionError : public Error
    {
        protected:
            ConstructionError(std::string ename, std::string msg, int exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }
            ConstructionError(std::string ename, std::string msg, ExitCodes exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ConstructionError(std::string msg, ExitCodes exit_code)
                : Error("ConstructionError", std::move(msg), exit_code)
            {
            }
            ConstructionError(std::string msg, int exit_code) : Error("ConstructionError", std::move(msg), exit_code)
            {
            }
    };

    class IncorrectConstruction : public ConstructionError
    {
        protected:
            IncorrectConstruction(std::string ename, std::string msg, int exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }
            IncorrectConstruction(std::string ename, std::string msg, ExitCodes exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            IncorrectConstruction(std::string msg, ExitCodes exit_code)
                : ConstructionError("IncorrectConstruction", std::move(msg), exit_code)
            {
            }
            IncorrectConstruction(std::string msg, int exit_code)
                : ConstructionError("IncorrectConstruction", std::move(msg), exit_code)
            {
            }
            explicit IncorrectConstruction(std::string msg)
                : IncorrectConstruction("IncorrectConstruction", msg, ExitCodes::IncorrectConstruction)
            {
            }

            static IncorrectConstruction PositionalFlag(std::string name)
            {
                return IncorrectConstruction(name + ": Flags cannot be positional");
            }
            static IncorrectConstruction Set0Opt(std::string name)
            {
                return IncorrectConstruction(name + ": Cannot set 0 expected, use a flag instead");
            }
            static IncorrectConstruction SetFlag(std::string name)
            {
                return IncorrectConstruction(name + ": Cannot set an expected number for flags");
            }
            static IncorrectConstruction ChangeNotVector(std::string name)
            {
                return IncorrectConstruction(name + ": You can only change the expected arguments for vectors");
            }
            static IncorrectConstruction AfterMultiOpt(std::string name)
            {
                return IncorrectConstruction(
                    name + ": You can't change expected arguments after you've changed the multi option policy!");
            }
            static IncorrectConstruction MissingOption(std::string name)
            {
                return IncorrectConstruction("Option " + name + " is not defined");
            }
            static IncorrectConstruction MultiOptionPolicy(std::string name)
            {
                return IncorrectConstruction(name +
                                             ": multi_option_policy only works for flags and exact value options");
            }
    };

    class BadNameString : public ConstructionError
    {
        protected:
            BadNameString(std::string ename, std::string msg, int exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }
            BadNameString(std::string ename, std::string msg, ExitCodes exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            BadNameString(std::string msg, ExitCodes exit_code)
                : ConstructionError("BadNameString", std::move(msg), exit_code)
            {
            }
            BadNameString(std::string msg, int exit_code)
                : ConstructionError("BadNameString", std::move(msg), exit_code)
            {
            }
            explicit BadNameString(std::string msg) : BadNameString("BadNameString", msg, ExitCodes::BadNameString)
            {
            }

            static BadNameString OneCharName(std::string name)
            {
                return BadNameString("Invalid one char name: " + name);
            }
            static BadNameString MissingDash(std::string name)
            {
                return BadNameString("Long names strings require 2 dashes " + name);
            }
            static BadNameString BadLongName(std::string name)
            {
                return BadNameString("Bad long name: " + name);
            }
            static BadNameString BadPositionalName(std::string name)
            {
                return BadNameString("Invalid positional Name: " + name);
            }
            static BadNameString ReservedName(std::string name)
            {
                return BadNameString("Names '-','--','++' are reserved and not allowed as option names " + name);
            }
            static BadNameString MultiPositionalNames(std::string name)
            {
                return BadNameString("Only one positional name allowed, remove: " + name);
            }
    };

    class OptionAlreadyAdded : public ConstructionError
    {
        protected:
            OptionAlreadyAdded(std::string ename, std::string msg, int exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }
            OptionAlreadyAdded(std::string ename, std::string msg, ExitCodes exit_code)
                : ConstructionError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            OptionAlreadyAdded(std::string msg, ExitCodes exit_code)
                : ConstructionError("OptionAlreadyAdded", std::move(msg), exit_code)
            {
            }
            OptionAlreadyAdded(std::string msg, int exit_code)
                : ConstructionError("OptionAlreadyAdded", std::move(msg), exit_code)
            {
            }
            explicit OptionAlreadyAdded(std::string name)
                : OptionAlreadyAdded(name + " is already added", ExitCodes::OptionAlreadyAdded)
            {
            }

            static OptionAlreadyAdded Requires(std::string name, std::string other)
            {
                return {name + " requires " + other, ExitCodes::OptionAlreadyAdded};
            }
            static OptionAlreadyAdded Excludes(std::string name, std::string other)
            {
                return {name + " excludes " + other, ExitCodes::OptionAlreadyAdded};
            }
    };

    class ParseError : public Error
    {
        protected:
            ParseError(std::string ename, std::string msg, int exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }
            ParseError(std::string ename, std::string msg, ExitCodes exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ParseError(std::string msg, ExitCodes exit_code) : Error("ParseError", std::move(msg), exit_code)
            {
            }
            ParseError(std::string msg, int exit_code) : Error("ParseError", std::move(msg), exit_code)
            {
            }
    };

    class Success : public ParseError
    {
        protected:
            Success(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            Success(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            Success(std::string msg, ExitCodes exit_code) : ParseError("Success", std::move(msg), exit_code)
            {
            }
            Success(std::string msg, int exit_code) : ParseError("Success", std::move(msg), exit_code)
            {
            }
            Success() : Success("Successfully completed, should be caught and quit", ExitCodes::Success)
            {
            }
    };

    class CallForHelp : public Success
    {
        protected:
            CallForHelp(std::string ename, std::string msg, int exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }
            CallForHelp(std::string ename, std::string msg, ExitCodes exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            CallForHelp(std::string msg, ExitCodes exit_code) : Success("CallForHelp", std::move(msg), exit_code)
            {
            }
            CallForHelp(std::string msg, int exit_code) : Success("CallForHelp", std::move(msg), exit_code)
            {
            }
            CallForHelp() : CallForHelp("This should be caught in your main function, see examples", ExitCodes::Success)
            {
            }
    };

    class CallForAllHelp : public Success
    {
        protected:
            CallForAllHelp(std::string ename, std::string msg, int exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }
            CallForAllHelp(std::string ename, std::string msg, ExitCodes exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            CallForAllHelp(std::string msg, ExitCodes exit_code) : Success("CallForAllHelp", std::move(msg), exit_code)
            {
            }
            CallForAllHelp(std::string msg, int exit_code) : Success("CallForAllHelp", std::move(msg), exit_code)
            {
            }
            CallForAllHelp()
                : CallForAllHelp("This should be caught in your main function, see examples", ExitCodes::Success)
            {
            }
    };

    class CallForVersion : public Success
    {
        protected:
            CallForVersion(std::string ename, std::string msg, int exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }
            CallForVersion(std::string ename, std::string msg, ExitCodes exit_code)
                : Success(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            CallForVersion(std::string msg, ExitCodes exit_code) : Success("CallForVersion", std::move(msg), exit_code)
            {
            }
            CallForVersion(std::string msg, int exit_code) : Success("CallForVersion", std::move(msg), exit_code)
            {
            }
            CallForVersion()
                : CallForVersion("This should be caught in your main function, see examples", ExitCodes::Success)
            {
            }
    };

    class RuntimeError : public ParseError
    {
        protected:
            RuntimeError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            RuntimeError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            RuntimeError(std::string msg, ExitCodes exit_code) : ParseError("RuntimeError", std::move(msg), exit_code)
            {
            }
            RuntimeError(std::string msg, int exit_code) : ParseError("RuntimeError", std::move(msg), exit_code)
            {
            }
            explicit RuntimeError(int exit_code = 1) : RuntimeError("Runtime error", exit_code)
            {
            }
    };

    class FileError : public ParseError
    {
        protected:
            FileError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            FileError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            FileError(std::string msg, ExitCodes exit_code) : ParseError("FileError", std::move(msg), exit_code)
            {
            }
            FileError(std::string msg, int exit_code) : ParseError("FileError", std::move(msg), exit_code)
            {
            }
            explicit FileError(std::string msg) : FileError("FileError", msg, ExitCodes::FileError)
            {
            }

            static FileError Missing(std::string name)
            {
                return FileError(name + " was not readable (missing?)");
            }
    };

    class ConversionError : public ParseError
    {
        protected:
            ConversionError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ConversionError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ConversionError(std::string msg, ExitCodes exit_code)
                : ParseError("ConversionError", std::move(msg), exit_code)
            {
            }
            ConversionError(std::string msg, int exit_code) : ParseError("ConversionError", std::move(msg), exit_code)
            {
            }
            explicit ConversionError(std::string msg)
                : ConversionError("ConversionError", msg, ExitCodes::ConversionError)
            {
            }

            ConversionError(std::string member, std::string name)
                : ConversionError("The value " + member + " is not an allowed value for " + name)
            {
            }
            ConversionError(std::string name, std::vector<std::string> results)
                : ConversionError("Could not convert: " + name + " = " + detail::join(results))
            {
            }

            static ConversionError TooManyInputsFlag(std::string name)
            {
                return ConversionError(name + ": too many inputs for a flag");
            }
            static ConversionError TrueFalse(std::string name)
            {
                return ConversionError(name + ": Should be true/false or a number");
            }
    };

    class ValidationError : public ParseError
    {
        protected:
            ValidationError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ValidationError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ValidationError(std::string msg, ExitCodes exit_code)
                : ParseError("ValidationError", std::move(msg), exit_code)
            {
            }
            ValidationError(std::string msg, int exit_code) : ParseError("ValidationError", std::move(msg), exit_code)
            {
            }
            explicit ValidationError(std::string msg)
                : ValidationError("ValidationError", msg, ExitCodes::ValidationError)
            {
            }
            explicit ValidationError(std::string name, std::string msg) : ValidationError(name + ": " + msg)
            {
            }
    };

    class RequiredError : public ParseError
    {
        protected:
            RequiredError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            RequiredError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            RequiredError(std::string msg, ExitCodes exit_code) : ParseError("RequiredError", std::move(msg), exit_code)
            {
            }
            RequiredError(std::string msg, int exit_code) : ParseError("RequiredError", std::move(msg), exit_code)
            {
            }
            explicit RequiredError(std::string name) : RequiredError(name + " is required", ExitCodes::RequiredError)
            {
            }

            static RequiredError Subcommand(std::size_t min_subcom)
            {
                if (min_subcom == 1)
                {
                    return RequiredError("A subcommand");
                }
                return {"Requires at least " + std::to_string(min_subcom) + " subcommands", ExitCodes::RequiredError};
            }
            static RequiredError Option(std::size_t min_option,
                                        std::size_t max_option,
                                        std::size_t used,
                                        const std::string &option_list)
            {
                if ((min_option == 1) && (max_option == 1) && (used == 0))
                    return RequiredError("Exactly 1 option from [" + option_list + "]");
                if ((min_option == 1) && (max_option == 1) && (used > 1))
                {
                    return {"Exactly 1 option from [" + option_list + "] is required but " + std::to_string(used) +
                                " were given",
                            ExitCodes::RequiredError};
                }
                if ((min_option == 1) && (used == 0))
                    return RequiredError("At least 1 option from [" + option_list + "]");
                if (used < min_option)
                {
                    return {"Requires at least " + std::to_string(min_option) + " options used but only " +
                                std::to_string(used) + " were given from [" + option_list + "]",
                            ExitCodes::RequiredError};
                }
                if (max_option == 1)
                    return {"Requires at most 1 options be given from [" + option_list + "]", ExitCodes::RequiredError};
                return {"Requires at most " + std::to_string(max_option) + " options be used but " +
                            std::to_string(used) + " were given from [" + option_list + "]",
                        ExitCodes::RequiredError};
            }
    };

    class ArgumentMismatch : public ParseError
    {
        protected:
            ArgumentMismatch(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ArgumentMismatch(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ArgumentMismatch(std::string msg, ExitCodes exit_code)
                : ParseError("ArgumentMismatch", std::move(msg), exit_code)
            {
            }
            ArgumentMismatch(std::string msg, int exit_code) : ParseError("ArgumentMismatch", std::move(msg), exit_code)
            {
            }
            explicit ArgumentMismatch(std::string msg)
                : ArgumentMismatch("ArgumentMismatch", msg, ExitCodes::ArgumentMismatch)
            {
            }

            ArgumentMismatch(std::string name, int expected, std::size_t received)
                : ArgumentMismatch(expected > 0 ? ("Expected exactly " + std::to_string(expected) + " arguments to " +
                                                   name + ", got " + std::to_string(received))
                                                : ("Expected at least " + std::to_string(-expected) + " arguments to " +
                                                   name + ", got " + std::to_string(received)),
                                   ExitCodes::ArgumentMismatch)
            {
            }

            static ArgumentMismatch AtLeast(std::string name, int num, std::size_t received)
            {
                return ArgumentMismatch(name + ": At least " + std::to_string(num) + " required but received " +
                                        std::to_string(received));
            }
            static ArgumentMismatch AtMost(std::string name, int num, std::size_t received)
            {
                return ArgumentMismatch(name + ": At most " + std::to_string(num) + " required but received " +
                                        std::to_string(received));
            }
            static ArgumentMismatch TypedAtLeast(std::string name, int num, std::string type)
            {
                return ArgumentMismatch(name + ": " + std::to_string(num) + " required " + type + " missing");
            }
            static ArgumentMismatch FlagOverride(std::string name)
            {
                return ArgumentMismatch(name + " was given a disallowed flag override");
            }
            static ArgumentMismatch PartialType(std::string name, int num, std::string type)
            {
                return ArgumentMismatch(name + ": " + type + " only partially specified: " + std::to_string(num) +
                                        " required for each element");
            }
    };

    class RequiresError : public ParseError
    {
        protected:
            RequiresError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            RequiresError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            RequiresError(std::string msg, ExitCodes exit_code) : ParseError("RequiresError", std::move(msg), exit_code)
            {
            }
            RequiresError(std::string msg, int exit_code) : ParseError("RequiresError", std::move(msg), exit_code)
            {
            }
            RequiresError(std::string curname, std::string subname)
                : RequiresError(curname + " requires " + subname, ExitCodes::RequiresError)
            {
            }
    };

    class ExcludesError : public ParseError
    {
        protected:
            ExcludesError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ExcludesError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ExcludesError(std::string msg, ExitCodes exit_code) : ParseError("ExcludesError", std::move(msg), exit_code)
            {
            }
            ExcludesError(std::string msg, int exit_code) : ParseError("ExcludesError", std::move(msg), exit_code)
            {
            }
            ExcludesError(std::string curname, std::string subname)
                : ExcludesError(curname + " excludes " + subname, ExitCodes::ExcludesError)
            {
            }
    };

    class ExtrasError : public ParseError
    {
        protected:
            ExtrasError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ExtrasError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ExtrasError(std::string msg, ExitCodes exit_code) : ParseError("ExtrasError", std::move(msg), exit_code)
            {
            }
            ExtrasError(std::string msg, int exit_code) : ParseError("ExtrasError", std::move(msg), exit_code)
            {
            }
            explicit ExtrasError(std::vector<std::string> args)
                : ExtrasError((args.size() > 1 ? "The following arguments were not expected: "
                                               : "The following argument was not expected: ") +
                                  detail::join(args, " "),
                              ExitCodes::ExtrasError)
            {
            }
            ExtrasError(const std::string &name, std::vector<std::string> args)
                : ExtrasError(name,
                              (args.size() > 1 ? "The following arguments were not expected: "
                                               : "The following argument was not expected: ") +
                                  detail::join(args, " "),
                              ExitCodes::ExtrasError)
            {
            }
    };

    class ConfigError : public ParseError
    {
        protected:
            ConfigError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            ConfigError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            ConfigError(std::string msg, ExitCodes exit_code) : ParseError("ConfigError", std::move(msg), exit_code)
            {
            }
            ConfigError(std::string msg, int exit_code) : ParseError("ConfigError", std::move(msg), exit_code)
            {
            }
            explicit ConfigError(std::string msg) : ConfigError("ConfigError", msg, ExitCodes::ConfigError)
            {
            }

            static ConfigError Extras(std::string item)
            {
                return ConfigError("INI was not able to parse " + item);
            }
            static ConfigError NotConfigurable(std::string item)
            {
                return ConfigError(item + ": This option is not allowed in a configuration file");
            }
    };

    class InvalidError : public ParseError
    {
        protected:
            InvalidError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            InvalidError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            InvalidError(std::string msg, ExitCodes exit_code) : ParseError("InvalidError", std::move(msg), exit_code)
            {
            }
            InvalidError(std::string msg, int exit_code) : ParseError("InvalidError", std::move(msg), exit_code)
            {
            }
            explicit InvalidError(std::string name)
                : InvalidError(name + ": Too many positional arguments with unlimited expected args",
                               ExitCodes::InvalidError)
            {
            }
    };

    class HorribleError : public ParseError
    {
        protected:
            HorribleError(std::string ename, std::string msg, int exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }
            HorribleError(std::string ename, std::string msg, ExitCodes exit_code)
                : ParseError(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            HorribleError(std::string msg, ExitCodes exit_code) : ParseError("HorribleError", std::move(msg), exit_code)
            {
            }
            HorribleError(std::string msg, int exit_code) : ParseError("HorribleError", std::move(msg), exit_code)
            {
            }
            explicit HorribleError(std::string msg) : HorribleError("HorribleError", msg, ExitCodes::HorribleError)
            {
            }
    };

    class OptionNotFound : public Error
    {
        protected:
            OptionNotFound(std::string ename, std::string msg, int exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }
            OptionNotFound(std::string ename, std::string msg, ExitCodes exit_code)
                : Error(std::move(ename), std::move(msg), exit_code)
            {
            }

        public:
            OptionNotFound(std::string msg, ExitCodes exit_code) : Error("OptionNotFound", std::move(msg), exit_code)
            {
            }
            OptionNotFound(std::string msg, int exit_code) : Error("OptionNotFound", std::move(msg), exit_code)
            {
            }
            explicit OptionNotFound(std::string name) : OptionNotFound(name + " not found", ExitCodes::OptionNotFound)
            {
            }
    };

} // namespace CLI
