#include <eosio/eosio.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/system.hpp>
#include <eosio/crypto.hpp>
#include <eosio/asset.hpp>
#include <eosio/transaction.hpp>
#include <eosio/action.hpp>
#include <eosio.token.hpp>

#define BASIC_SYMBOL symbol("TWIG", 4)
#define EOS_SYMBOL symbol("EOS", 4)

using namespace eosio;
using namespace std;
using std::string;
using std::vector;

/*

tokentestac1
[ { "quantity": "500.0000 TWIG", "date": 60 }, { "quantity": "500.0000 TWIG", "date": 120 } ]

eosio-cpp -o /var/cff_server/dapps/eostoken/eosio.token.wasm /var/cff_server/dapps/eostoken/eosio.token.cpp -I=/var/cff_server/dapps/eostoken  -abigen -abigen_output=/var/cff_server/dapps/eostoken/eosio.token.abi -contract=token
*/

namespace eosio {

void token::create( const name&   issuer,
                    const asset&  maximum_supply )
{
    require_auth( get_self() );

    auto sym = maximum_supply.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( maximum_supply.is_valid(), "Invalid supply");
    check( maximum_supply.amount > 0, "Max-Supply must be more then 0");

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing == statstable.end(), "This token already exist" );

    statstable.emplace( get_self(), [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( const name& to, const asset& quantity, const string& memo )
{
	getblacklist( to );
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "This token dont exist." );
    const auto& st = *existing;
    check( to == st.issuer, "Token can only be issued TO issuer account" );
	require_recipient( to );
    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( quantity.amount <= st.max_supply.amount - st.supply.amount, "Quantity exceeds available supply");

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply += quantity;
    });
    add_balance( st.issuer, quantity, st.issuer );
}

void token::retire( const asset& quantity, const string& memo )
{
    auto sym = quantity.symbol;
    check( sym.is_valid(), "Invalid symbol name" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    stats statstable( get_self(), sym.code().raw() );
    auto existing = statstable.find( sym.code().raw() );
    check( existing != statstable.end(), "Token with symbol does not exist" );
    const auto& st = *existing;

    require_auth( st.issuer );
    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount should be more then 0" );

    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );

    statstable.modify( st, same_payer, [&]( auto& s ) {
       s.supply -= quantity;
    });
    sub_balance( st.issuer, quantity );
}

void token::transfer( name from, name to, asset quantity, string memo )
{
    check( from != to, "Cannot transfer to self" );
    require_auth( from );
	getblacklist( from );
	getblacklist( to );
    check( is_account( to ), "TO ["+to.to_string()+"] account does not exist");
    auto sym = quantity.symbol.code();
    stats statstable( get_self(), sym.raw() );
    const auto& st = statstable.get( sym.raw() );

    require_recipient( from );
    require_recipient( to );

    check( quantity.is_valid(), "Invalid quantity" );
    check( quantity.amount > 0, "Amount less then 0 ["+std::to_string( quantity.amount )+"]" );
    check( quantity.symbol == st.supply.symbol, "Symbol precision mismatch" );
    check( memo.size() <= 256, "Memo has more than 256 bytes" );

    auto payer = has_auth( to ) ? to : from;

	sub_balance( from, quantity );
	add_balance( to, quantity, payer );
}

void token::sub_balance( const name owner, const asset value ) {
	accounts from_acnts( get_self(), owner.value );
	const auto& from = from_acnts.find( value.symbol.code().raw() );
	if( from == from_acnts.end() ) {
		check( false, "FROM ["+owner.to_string()+"] dont have ["+value.symbol.code().to_string()+"] tokens" );
	}else{
		check( from->balance.amount >= value.amount, "Overdraw balance on token ["+value.symbol.code().to_string()+"] on ["+owner.to_string()+"]" );

		if( value.symbol == BASIC_SYMBOL ){
			getfroze( owner, from->balance, value );
		}

		from_acnts.modify( from, owner, [&]( auto& a ) {
			a.balance -= value;
		});
	}
}

void token::add_balance( const name owner, const asset value, const name ram_payer )
{
   accounts to_acnts( get_self(), owner.value );
   auto to = to_acnts.find( value.symbol.code().raw() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, same_payer, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

void token::open( const name& owner, const symbol& symbol, const name& ram_payer )
{
   require_auth( ram_payer );
   getblacklist( owner );
   getblacklist( ram_payer );

   check( is_account( owner ), "owner ["+owner.to_string()+"] account does not exist" );

   auto sym_code_raw = symbol.code().raw();
   stats statstable( get_self(), sym_code_raw );
   const auto& st = statstable.get( sym_code_raw, "Symbol does not exist" );
   check( st.supply.symbol == symbol, "Symbol precision mismatch" );

   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( sym_code_raw );
   if( it == acnts.end() ) {
      acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = asset{0, symbol};
      });
   }
}

void token::close( const name& owner, const symbol& symbol )
{
   require_auth( owner );
   getblacklist( owner );
   accounts acnts( get_self(), owner.value );
   auto it = acnts.find( symbol.code().raw() );
   check( it != acnts.end(), "Balance row already deleted or never existed. Action won't have any effect." );
   check( it->balance.amount == 0, "Cannot close because the balance is not zero." );
   acnts.erase( it );
}

void token::getblacklist( name account ){
	db_blacklist blacklist(get_self(), get_self().value);
	auto black = blacklist.find( account.value );
	if(black != blacklist.end()){
		check(false, "Account is on BLACKLIST");
	}
}

void token::blacklist( name account, bool a ){
	require_auth(get_self());
    require_recipient( account );
	if( account == get_self()){
		check(false, "SELF not should be added");
	}
	db_blacklist blacklist(get_self(), get_self().value);
	auto black = blacklist.find( account.value );
	if(black == blacklist.end()){
		if(!a){ check(false, "Account dont exist"); }
		blacklist.emplace(get_self(), [&](auto& t){
			t.account = account;
		});
	}else{
		if(a){
			check(false, "Account already exist");
		}else{
			blacklist.erase(black);
		}
	}
}

void token::frozen( name account, vector<token::listFroze> list ){
	require_auth(get_self());
    require_recipient( account );
	getblacklist( account );
	if( account == get_self()){
		check(false, "SELF not should be added");
	}
	uint64_t time = current_time_point().sec_since_epoch();
	db_frozelist frozelist(get_self(), get_self().value);
	auto froze = frozelist.find( account.value );
	if(froze == frozelist.end()){
		for (int i = 0; i < list.size(); i++) {
			list[i].date += time;
		}
		frozelist.emplace(get_self(), [&](auto& t){
			t.account = account;
			t.froze = list;
		});
	}else{
		vector<token::listFroze> exist = froze->froze;
		vector<token::listFroze> updated;
		for (int i = 0; i < exist.size(); i++) {
			if(exist[i].date > time){
				updated.emplace_back(exist[i]);
			}
		}
		for (int i = 0; i < list.size(); i++) {
			list[i].date += time;
			updated.emplace_back(list[i]);
		}
		frozelist.modify(froze, get_self(), [&](auto& t){
			t.froze = updated;
		});
	}
}

void token::getfroze( name account, asset balance, asset transfered ){
	db_frozelist frozelist(get_self(), get_self().value);
	auto froze = frozelist.find( account.value );
	if(froze != frozelist.end()){
		uint64_t time = current_time_point().sec_since_epoch();
		uint64_t total_locked = 0;
		uint64_t time_locked = 0;
		uint64_t total_avialable = balance.amount;
		//vector<token::listFroze> updated;
		for (int i = 0; i < froze->froze.size(); i++) {
			if(froze->froze[i].date > time ){
				total_locked += froze->froze[i].quantity.amount;
				//updated.emplace_back(froze->froze[i]);
			}
		}
		if( total_locked > 0 ){
			if( (total_avialable - total_locked) <= transfered.amount || total_avialable <= total_locked ){
				check(false, "Amount is froze. Try transfer less");
			}
		}
		/*	IF NEED CLEAN OLD DATELOCK
		frozelist.modify(froze, get_self(), [&](auto& t){
			t.froze = updated;
		});
		*/
	}
}

void token::buyout( name from, asset quantity, float rate, string memo ){
	require_auth(get_self());
    require_recipient( from );
	getblacklist( from );
	if( from == get_self()){
		check(false, "SELF not can redeem from SELF");
	}
	asset RECEIVED;
	RECEIVED.symbol = EOS_SYMBOL;

    check( memo.size() <= 256, "Memo has more than 256 bytes" );

	accounts from_acnts( get_self(), from.value );
	const auto& fromdb = from_acnts.find( quantity.symbol.code().raw() );
	if( fromdb == from_acnts.end() ) {
		check( false, "FROM ["+from.to_string()+"] dont have ["+quantity.symbol.code().to_string()+"] tokens" );
	}else{
		check( fromdb->balance.amount >= quantity.amount, "Overdraw balance on token ["+quantity.symbol.code().to_string()+"] on ["+from.to_string()+"]" );

		if( quantity.symbol != BASIC_SYMBOL ){
			check( false, "Only ["+BASIC_SYMBOL.code().to_string()+"] token can be redeem!" );
		}

		if( quantity.amount == 0 ){
			float receive = rate * fromdb->balance.amount;
			RECEIVED.amount = receive;

			from_acnts.modify( fromdb, from, [&]( auto& a ) {
				a.balance -= fromdb->balance;
			});
			add_balance( get_self(), fromdb->balance, get_self() );

		}else{
			float receive = rate * quantity.amount;
			RECEIVED.amount = receive;

			from_acnts.modify( fromdb, from, [&]( auto& a ) {
				a.balance -= quantity;
			});
			add_balance( get_self(), quantity, get_self() );

		}

		action(
			permission_level{ get_self(), "active"_n },
			"eosio.token"_n, "transfer"_n,
			std::make_tuple( get_self(), from, RECEIVED, std::string( "redeem" ))
		).send();
	}
}

} /// namespace eosio
