#pragma once
#include<comedian.h>
#include<stdexcept>

/**
 * \file spede.h
 *
 * Spede definition.
 */

namespace Comedy {

    /**
     * Spede is the funniest person in the world.
     */
    class Spede : public Comedian {
    public:
        /**
         * Creates a new spede.
         */
        Spede();

        /**
         * Make him do the funny thing he is known for.
         */
        void slap_forehead();

        virtual void tell_joke() {
            throw std::runtime_error("Not implemented");
        }

    private:
        int num_movies; ///< How many movies has he done.
    };
}
