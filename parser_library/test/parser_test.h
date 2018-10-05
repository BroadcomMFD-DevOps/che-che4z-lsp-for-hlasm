#ifndef HLASMPLUGIN_HLASMPARSERLIBARY_PARSER_TEST_H
#define HLASMPLUGIN_HLASMPARSERLIBARY_PARSER_TEST_H

#include "common_testing.h"

class library_test : public testing::Test
{
public:
	virtual void SetUp(std::string param)
	{
		holder = std::make_unique<parser_holder>(get_content("test/library/input/" + param + ".in"));
	}
	virtual void TearDown() {}
protected:
	std::unique_ptr<parser_holder> holder;
};

TEST_F(library_test, expression_test)
{
	std::string tcase = "expression_test";

	SetUp(tcase);
	auto t = holder->parser->expr_test()->test_str;
	ASSERT_EQ(t, get_content("test/library/output/" + tcase + ".output"));

	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}

//3 instruction statements and 3 EOLLN
TEST_F(library_test, simple)
{
	std::string tcase = "simple";

	SetUp(tcase);

	//compare tokens with output file

	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}

//5 instruction statements that have 1,1,1,2 and 4 operands respectively
TEST_F(library_test, operand)
{
	std::string tcase = "operand";

	SetUp(tcase);

	//compare tokens with output file

	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);

}

//3 alternative forms of instructions
TEST_F(library_test, continuation)
{
	std::string tcase = "continuation";

	SetUp(tcase);

	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}
//#define CORRECTNESS_TEST
#ifdef CORRECTNESS_TEST
//simply parse correctly
TEST_F(library_test, correctness)
{
	std::string tcase = "correctness";

	SetUp(tcase);

	//compare tokens with output file
	//ASSERT_EQ(token_string, get_content("test/library/output/tokens/" + tcase + ".output"));

	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}
#endif

//finding 3 variable symbols in model statement
TEST_F(library_test, model_statement)
{
	std::string tcase = "model_statement";

	SetUp(tcase);


	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}

//simply parse correctly
TEST_F(library_test, comment)
{
	std::string tcase = "comment";

	SetUp(tcase);

	//compare tokens with output file

	holder->parser->program();
	//no errors found while parsing
	ASSERT_EQ(holder->parser->getNumberOfSyntaxErrors(), size_t_zero);
}
#endif // !HLASMPLUGIN_HLASMPARSERLIBARY_PARSER_TEST_H
