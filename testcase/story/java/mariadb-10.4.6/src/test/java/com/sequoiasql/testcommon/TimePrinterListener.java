package com.sequoiasql.testcommon;

import java.text.SimpleDateFormat;
import java.util.Date;

import org.testng.ITestResult;
import org.testng.TestListenerAdapter;

/**
 * Created by laojingtang on 17-11-23.
 */
public class TimePrinterListener extends TestListenerAdapter {

    @Override
    public void onTestFailure( ITestResult itr ) {
        System.out.println( "runGroup"
                + itr.getMethod().getXmlTest().getIncludedGroups().toString()
                + " " + itr.getMethod().getTestClass().getRealClass()
                + " failed" );
        super.onTestFailure( itr );
    }

    @Override
    public void onTestFailedButWithinSuccessPercentage( ITestResult itr ) {
        super.onTestFailedButWithinSuccessPercentage( itr );

    }

    @Override
    public void onConfigurationSuccess( ITestResult itr ) {
        super.onConfigurationSuccess( itr );
        if ( itr.getMethod().isAfterClassConfiguration() ) {
            printEndTime( itr );
        }
    }

    @Override
    public void onConfigurationFailure( ITestResult itr ) {
        super.onConfigurationFailure( itr );
        if ( itr.getMethod().isAfterClassConfiguration() ) {
            printEndTime( itr );
        }
    }

    @Override
    public void onConfigurationSkip( ITestResult itr ) {
        super.onConfigurationSkip( itr );
        if ( itr.getMethod().isAfterClassConfiguration() ) {
            printEndTime( itr );
        }
    }

    @Override
    public void beforeConfiguration( ITestResult tr ) {
        super.beforeConfiguration( tr );
        if ( tr.getMethod().isBeforeClassConfiguration() ) {
            printBeginTime( tr );
        }
    }

    private void printBeginTime( ITestResult tr ) {
        System.out.println( getCurTimeStr() + "Begin testcase: "
                + getTestMethodName( tr ) );
    }

    private void printEndTime( ITestResult tr ) {
        System.out.println(
                getCurTimeStr() + "End testcase: " + getTestMethodName( tr ) );
    }

    private String getTestMethodName( ITestResult tr ) {
        return tr.getTestClass().getRealClass().getName();
    }

    private String getCurTimeStr() {
        return new SimpleDateFormat( "yyyy-MM-dd HH:mm:ss:S" )
                .format( new Date() );
    }

}
