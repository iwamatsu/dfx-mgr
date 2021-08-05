// buffer.hpp
#ifndef BUFFER_HPP_
#define BUFFER_HPP_

#include <string>

namespace opendfx {

class Buffer {

	public:
		explicit Buffer(const std::string &name);
		explicit Buffer(const std::string &name, const std::string &strid);
		std::string info() const;
		std::string getName() const;
		int addLinkRefCount();
		int subsLinkRefCount();
		int getLinkRefCount();
		inline bool operator==(const Buffer& rhs) const {
			std::cout << info() <<  " : " << rhs.info() << "\n";
			std::cout << this->name <<  " : " << rhs.name << "\n";
		    return (this->id == rhs.id && this->strid == rhs.strid);
		}
		int setDeleteFlag(bool deleteFlag);
		bool getDeleteFlag() const;
		static inline bool staticGetDeleteFlag(Buffer *buffer) {
			return buffer->getDeleteFlag();
		}
		inline std::string getId() const { return this->strid; }
		std::string toJson(bool withDetail = false);
	private:
		std::string name;
		int id;
		std::string strid;
		int linkRefCount;
		bool deleteFlag;
	};
} // #end of wrapper
#endif // BUFFER_HPP_
