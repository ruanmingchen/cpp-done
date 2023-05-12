//
// Created by root on 23-4-4.
//

#ifndef A_OUT_LIBRARY_H
#define A_OUT_LIBRARY_H

#include <boost/dll.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/system/error_code.hpp>
#include <boost/utility/string_view.hpp>

class Library
{
public:
    explicit Library(boost::string_view filename) : filename_(filename.data()){};

    Library(Library const&) = delete;
    Library& operator=(Library const&) = delete;

    virtual ~Library()
    {
        if (handle_.is_loaded()) {
            handle_.unload();
        }
    }

    bool Load(std::string& err)
    {
        err.clear();
        bool ret = true;
        try {
            if (!handle_.is_loaded()) {
                handle_.load(filename_, boost::dll::load_mode::rtld_global | boost::dll::load_mode::rtld_now);
            }
        } catch (const boost::system::system_error& ex) {
            ret = false;
            err = ex.what();
        } catch (const std::exception& e) {
            ret = false;
            err = e.what();
        }

        return ret;
    };

    template <class PTR_T, class FUNC_T>
    PTR_T GetFunctionPoint(boost::string_view func_name, std::string& err)
    {
        err.clear();
        try {
            if (handle_.is_loaded()) {
                auto func = handle_.get<FUNC_T>(func_name.data());
                return func;
            }
        } catch (const boost::system::system_error& ex) {
            err = ex.code().message();
        } catch (const std::exception& e) {
            err = e.what();
        }

        return nullptr;
    }

private:
    std::string filename_{};
    boost::dll::shared_library handle_{};
};

#endif  // A_OUT_LIBRARY_H
