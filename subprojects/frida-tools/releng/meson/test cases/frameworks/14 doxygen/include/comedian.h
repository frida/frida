#pragma once

namespace Comedy {

    /**
     * Interface for a funnyperson.
     */
    class Comedian {
    public:
        /**
         * Do the thing people want to happen.
         */
        virtual void tell_joke() = 0;
        virtual ~Comedian(){};
    };

}
