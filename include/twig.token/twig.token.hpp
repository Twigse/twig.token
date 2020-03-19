#pragma once

#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/action.hpp>
#include <eosio.token.hpp>

using namespace eosio;
using namespace std;
using std::string;
using std::vector;

namespace eosiosystem {
	class system_contract;
}

namespace eosio {

	using std::string;

	class [[eosio::contract("token")]] token : public contract {
		public:
			using contract::contract;

		[[eosio::action]]
		void create( const name& issuer, const asset& maximum_supply);

		[[eosio::action]]
		void issue( const name& to, const asset& quantity, const string& memo );

		[[eosio::action]]
		void retire( const asset& quantity, const string& memo );

		[[eosio::action]]
		void transfer( const name from, const name to, const asset quantity, const string memo );

		[[eosio::action]]
		void open( const name& owner, const symbol& symbol, const name& ram_payer );

		[[eosio::action]]
		void close( const name& owner, const symbol& symbol );

		[[eosio::action]]
		void blacklist( name account, bool a );

		[[eosio::action]]
		void buyout( name from, asset quantity, float rate, string memo );

		struct listFroze {
			asset quantity;
			uint64_t date;
			EOSLIB_SERIALIZE( listFroze, (quantity)(date) )
		};
		listFroze newFroze;

		//[{"quantity":"500.0000 TWIG","date":"120"}]
		[[eosio::action]]
		void frozen( name account, vector<token::listFroze> list );

		static asset get_supply( const name& token_contract_account, const symbol_code& sym_code )
		{
			stats statstable( token_contract_account, sym_code.raw() );
			const auto& st = statstable.get( sym_code.raw() );
			return st.supply;
		}

		static asset get_balance( const name& token_contract_account, const name& owner, const symbol_code& sym_code )
		{
			accounts accountstable( token_contract_account, owner.value );
			const auto& ac = accountstable.get( sym_code.raw() );
			return ac.balance;
		}

		using create_action = eosio::action_wrapper<"create"_n, &token::create>;
		using issue_action = eosio::action_wrapper<"issue"_n, &token::issue>;
		using retire_action = eosio::action_wrapper<"retire"_n, &token::retire>;
		using transfer_action = eosio::action_wrapper<"transfer"_n, &token::transfer>;
		using open_action = eosio::action_wrapper<"open"_n, &token::open>;
		using close_action = eosio::action_wrapper<"close"_n, &token::close>;
		using blacklist_action = eosio::action_wrapper<"blacklist"_n, &token::blacklist>;
		using froze_action = eosio::action_wrapper<"froze"_n, &token::frozen>;
		using buyout_action = eosio::action_wrapper<"buyout"_n, &token::buyout>;

		private:

		struct [[eosio::table]] blacklists {
			name account;
			uint64_t primary_key() const { return account.value; }
			EOSLIB_SERIALIZE( blacklists, (account) )
		};
		typedef eosio::multi_index< "blacklist"_n, blacklists > db_blacklist;
		void getblacklist( name account );

		struct [[eosio::table]] frozelist {
			name account;
			vector<listFroze> froze;
			uint64_t primary_key() const { return account.value; }
			EOSLIB_SERIALIZE( frozelist, (account)(froze) )
		};
		typedef eosio::multi_index< "frozelist"_n, frozelist > db_frozelist;
		void getfroze( name account, asset balance, asset transfered );

		struct [[eosio::table]] account {
			asset	balance;

			uint64_t primary_key()const { return balance.symbol.code().raw(); }
		};

		struct [[eosio::table]] currency_stats {
			asset	supply;
			asset	max_supply;
			name	issuer;

			uint64_t primary_key()const { return supply.symbol.code().raw(); }
		};

		typedef eosio::multi_index< "accounts"_n, account > accounts;
		typedef eosio::multi_index< "stat"_n, currency_stats > stats;

		void sub_balance( const name owner, const asset value );
		void add_balance( const name owner, const asset value, const name ram_payer );
	};
}
